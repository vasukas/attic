#include "ipclib/Queue.h"

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

using namespace boost::interprocess;

namespace ipclib
{

/*

	memory layout:
	   Sync object
	   messages, array of:
		   Header object
		   bytes
	
	Writer simply writes message to the end of the array.
	
	Reader may read message or return on cancel event.
	Cancel events are caused by:
	- QueueConsumer::cancel_read()
	- QueueConsumer destructor
	- last writer being destroyed
	
	Cancel events are sent to all readers and checked by UID;
	if it doesn't match, reader continues to wait.
	
	Event on last writer being destroyed intended for all readers,
	so it's checked by mismatch between global () event counter
	and one in the reader object.

*/

class QueueInternal {
public:
    using ReadRet = QueueConsumer::ReadRet;

    template <typename CreateType>
    void create(const std::string& name) {
        shm = shared_memory_object(CreateType{}, name.c_str(), read_write);
        shm.truncate(sync_size); // resize

        resize_mapping(0);
        new(sync) Sync(); // init mutexes and stuff
        sync->name = name;
    }
    void open(const std::string& name) {
		shm = shared_memory_object(open_only, name.c_str(), read_write);
        resize_mapping(0);
    }

    // add shm user
    std::pair<uint64_t, uint64_t> ref(bool is_producer) {
        scoped_lock<interprocess_mutex> lock(sync->mut);
        (is_producer ? sync->ref_producers : sync->ref_consumers) += 1;
        sync->uid_counter += 1;
        return {sync->uid_counter, sync->cancel_all_counter}; // uid value and cancel_all event counter
    }
    // remove shm user
    void deref(bool is_producer) {
        scoped_lock<interprocess_mutex> lock(sync->mut);
        (is_producer ? sync->ref_producers : sync->ref_consumers) -= 1;
        if (!sync->ref_producers) {
            cancel_all_reads(ReadRet::ReadNoProducersLeft);
        }
        if (!sync->ref_producers && !sync->ref_consumers) {
            remove_queue(sync->name); // only unlinks, so shm still exists
        }
    }

    void write(std::function<void(void *mem)> writer, size_t size) {
        scoped_lock<interprocess_mutex> lock(sync->mut);

        const offset_t mem_begin = sync->data_offset;
        const offset_t mem_end = sync->data_offset + size + header_size;
        offset_t shm_size = 0;
        if (!shm.get_size(shm_size)) { // that shouldn't happen
            throw std::runtime_error("QueueProducer::write() shared_memory_object::get_size() failed");
        }

        if (shm_size < mem_end) {
            shm.truncate(sync_size + mem_end); // resize
            resize_mapping(mem_end);
        }

        // write message
        auto ptr = static_cast<uint8_t*>(region.get_address()) + sync_size + mem_begin;
        static_cast<Header*>(static_cast<void*>(ptr))->size = size;
        writer(ptr + header_size);

        sync->data_offset = mem_end;
        sync->message.notify_one();
    }
    ReadRet read(uint64_t uid, uint64_t& last_cancel_all, std::function<void(const void *mem, size_t size)> reader) {
        auto on_cancel = [&]{
            if (sync->cancel != ReadRet::ReadOk && sync->cancel_uid == uid) {
                // unset cancel value and allow it to be set for other processes
                sync->cancel_free.notify_one();
                return std::exchange(sync->cancel, ReadRet::ReadOk);
            }
            if (sync->cancel_all_counter != last_cancel_all) {
                // another cancel_all event has happened since last one or object creation
                last_cancel_all = sync->cancel_all_counter;
                return sync->cancel_all;
            }
            return ReadRet::ReadOk;
        };
		
        scoped_lock<interprocess_mutex> lock(sync->mut);
        while (true) {
            if (sync->data_offset) {
                break;
            }
            if (auto ret = on_cancel()) {
                return ret;
            }
            sync->message.wait(lock);
            // cancel event for another process could have woken as up, so check conditions again
        }

        // resize data region if needed
        if (sync->data_offset > region.get_size()) {
            resize_mapping(sync->data_offset);
        }

        // read message
        auto ptr = static_cast<uint8_t*>(region.get_address()) + sync_size;
        size_t size = static_cast<Header*>(static_cast<void*>(ptr))->size;
        reader(ptr + header_size, size);

        // left-shift buffer
        sync->data_offset -= header_size + size;
        std::memmove(ptr, ptr + header_size + size, sync->data_offset);

        return ReadRet::ReadOk;
    }
    void cancel_read(uint64_t uid, ReadRet reason) {
        scoped_lock<interprocess_mutex> lock(sync->mut);
        if (sync->cancel != ReadRet::ReadOk) {
            // cancel event for some process already set, not yet used
            sync->cancel_free.wait(lock);
        }
        sync->cancel = reason;
        sync->cancel_uid = uid;
        sync->message.notify_all(); // have to wake all
    }
    void cancel_all_reads(ReadRet reason) {
        scoped_lock<interprocess_mutex> lock(sync->mut);
        sync->cancel_all_counter += 1;
        sync->cancel_all = reason;
        sync->message.notify_all();
    }

private:
    // synchronization block
    struct Sync {
        // data
        interprocess_mutex mut;
        interprocess_condition message; // also notified on cancel event
        size_t data_offset = 0; // where next message will be written
        std::string name;

        // refcount
        int ref_producers = 0;
        int ref_consumers = 0;
        uint64_t uid_counter = 0; // even if one object would be created each millisecond,
                                  // this counter won't wrap for 600 millions of years

        // cancel one
        ReadRet cancel = ReadRet::ReadOk;
        uint64_t cancel_uid;
        interprocess_condition cancel_free; // so multiple processes can cancel at the same time safely

        // cancel all
        uint64_t cancel_all_counter = 0; // used to detect new event
        ReadRet cancel_all;
    };

    // message header
    struct Header {
        size_t size; // byte size of message
    };

    static constexpr int sync_size = sizeof(Sync);
    static constexpr int header_size = sizeof(Header);

    shared_memory_object shm;
    mapped_region region;
    Sync* sync;

    void resize_mapping(size_t size) {
        region = mapped_region(shm, read_write, 0, sync_size + size);
        sync = static_cast<Sync*>(region.get_address());
    }
};


static std::unique_ptr<QueueInternal> create_queue(const std::string& name, bool allow_existing) {
    auto p = std::make_unique<QueueInternal>();
    if (allow_existing) {
        p->create<open_or_create_t>(name);
    }
    else {
        p->create<create_only_t>(name);
    }
    return p;
}
static std::unique_ptr<QueueInternal> open_queue(const std::string& name) {
    auto p = std::make_unique<QueueInternal>();
    p->open(name);
    return p;
}
void remove_queue(const std::string& name) noexcept {
    shared_memory_object::remove(name.c_str());
}


void QueueProducer::write_message(std::function<void(void *mem)> writer, size_t size) {
    return p->write(std::move(writer), size);
}
QueueProducer QueueProducer::create(const std::string& name, bool allow_existing) {
    return QueueProducer(create_queue(name, allow_existing));
}
QueueProducer QueueProducer::open(const std::string& name) {
    return QueueProducer(open_queue(name));
}
QueueProducer::QueueProducer(std::unique_ptr<QueueInternal> p): p(std::move(p)) {
    this->p->ref(true);
}
QueueProducer::~QueueProducer() noexcept {
    if (p) {
        p->deref(true);
    }
}
QueueProducer::QueueProducer(QueueProducer&&) noexcept = default;


std::error_code QueueConsumer::get_error_code(ReadRet ret) {
    switch (ret) {
        case ReadOk: return std::error_code{};
        case ReadCancelled: return std::make_error_code(std::errc::operation_canceled);
        case ReadDestroyed: return std::make_error_code(std::errc::owner_dead);
        case ReadNoProducersLeft: return std::make_error_code(std::errc::broken_pipe);
    }
    return std::make_error_code(std::errc::state_not_recoverable); // the end is near
}
QueueConsumer QueueConsumer::create(const std::string& name, bool allow_existing) {
    return QueueConsumer(create_queue(name, allow_existing));
}
QueueConsumer QueueConsumer::open(const std::string& name) {
    return QueueConsumer(open_queue(name));
}
QueueConsumer::ReadRet QueueConsumer::read_message(std::function<void(const void *mem, size_t size)> reader) {
    return p->read(uid, last_cancel_all, std::move(reader));
}
void QueueConsumer::cancel_read() {
    p->cancel_read(uid, ReadCancelled);
}
QueueConsumer::QueueConsumer(std::unique_ptr<QueueInternal> p): p(std::move(p)) {
    auto ret = this->p->ref(false);
    uid = ret.first;
    last_cancel_all = ret.second;
}
QueueConsumer::~QueueConsumer() noexcept {
    if (p) {
        p->cancel_read(uid, ReadDestroyed);
        p->deref(false);
    }
}
QueueConsumer::QueueConsumer(QueueConsumer&&) noexcept = default;

} // namespace ipclib
