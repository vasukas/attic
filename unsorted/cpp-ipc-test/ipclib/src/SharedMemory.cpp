#include "ipclib/SharedMemory.h"

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

using namespace boost::interprocess;

namespace ipclib
{

class SharedMemoryInternal {
public:
    struct Sync {
        interprocess_upgradable_mutex mut;
    };
    static constexpr int sync_size = sizeof(Sync);

    template <typename CreateType>
    void create(const std::string& name) {
        shm = shared_memory_object(CreateType{}, name.c_str(), read_write);
        resize(0);
        new(&get_sync()) Sync();
    }
    void open(const std::string& name) {
        shm = shared_memory_object(open_only, name.c_str(), read_write);
        update_mapping();
    }

	Sync& get_sync() {
		return *static_cast<Sync*>(region.get_address());
	}
	uint8_t* get_data() {
		return static_cast<uint8_t*>(region.get_address()) + sync_size;
	}
	size_t get_size() {
		return region.get_size() - sync_size;
	}
    void resize(size_t size) {
		shm.truncate(size + sync_size);
		update_mapping();
    }
	void update_mapping() {
		offset_t size = 0;
		if (!shm.get_size(size)) { // shouldn't happen
			throw std::runtime_error("SharedMemoryInternal::update_mapping() shm.get_size failed");
		}
		region = mapped_region(shm, read_write, 0, size);
	}
	
private:
	shared_memory_object shm;
    mapped_region region;
};

class SharedMemoryInternalRead {
public:
	SharedMemoryInternal& p;
	sharable_lock<interprocess_upgradable_mutex> lock;
	
	SharedMemoryInternalRead(SharedMemoryInternal& p): p(p), lock(p.get_sync().mut) {}
};

class SharedMemoryInternalWrite {
public:
	SharedMemoryInternal& p;
	scoped_lock<interprocess_upgradable_mutex> lock;
	
	SharedMemoryInternalWrite(SharedMemoryInternal& p): p(p), lock(p.get_sync().mut) {}
};


static std::unique_ptr<SharedMemoryInternal> create_shared_memory(const std::string& name, bool allow_existing) {
    auto p = std::make_unique<SharedMemoryInternal>();
    if (allow_existing) {
        p->create<open_or_create_t>(name);
    }
    else {
        p->create<create_only_t>(name);
    }
    return p;
}
static std::unique_ptr<SharedMemoryInternal> open_shared_memory(const std::string& name) {
    auto p = std::make_unique<SharedMemoryInternal>();
    p->open(name);
    return p;
}
void SharedMemory::remove(const std::string& name) noexcept {
    shared_memory_object::remove(name.c_str());
}


const uint8_t *SharedMemoryReadLock::data() const noexcept {
    return p->p.get_data();
}
size_t SharedMemoryReadLock::size() const noexcept {
    return p->p.get_size();
}
SharedMemoryReadLock::SharedMemoryReadLock(SharedMemoryInternal& p): p(std::make_unique<SharedMemoryInternalRead>(p)) {}
SharedMemoryReadLock::~SharedMemoryReadLock() noexcept {}
SharedMemoryReadLock::SharedMemoryReadLock(SharedMemoryReadLock&& other) noexcept = default;
SharedMemoryReadLock& SharedMemoryReadLock::operator=(SharedMemoryReadLock&& other) = default;


uint8_t *SharedMemoryWriteLock::data() const noexcept {
    return p->p.get_data();
}
size_t SharedMemoryWriteLock::size() const noexcept {
    return p->p.get_size();
}
SharedMemoryWriteLock::SharedMemoryWriteLock(SharedMemoryInternal& p): p(std::make_unique<SharedMemoryInternalWrite>(p)) {}
SharedMemoryWriteLock::~SharedMemoryWriteLock() noexcept {}
SharedMemoryWriteLock::SharedMemoryWriteLock(SharedMemoryWriteLock&& other) noexcept = default;
SharedMemoryWriteLock& SharedMemoryWriteLock::operator=(SharedMemoryWriteLock&& other) = default;


SharedMemory SharedMemory::create(const std::string& name, bool allow_existing) {
    return SharedMemory(create_shared_memory(name, allow_existing));
}
SharedMemory SharedMemory::open(const std::string& name) {
    return SharedMemory(open_shared_memory(name));
}
SharedMemory::~SharedMemory() noexcept
{}
size_t SharedMemory::size() const noexcept {
    return p->get_size();
}
void SharedMemory::resize(size_t new_size) {
    p->resize(new_size);
}
size_t SharedMemory::update_size() {
	p->update_mapping();
	return p->get_size();
}
SharedMemoryReadLock SharedMemory::read_lock() {
    return SharedMemoryReadLock(*p);
}
SharedMemoryWriteLock SharedMemory::write_lock() {
    return SharedMemoryWriteLock(*p);
}
SharedMemory::SharedMemory(std::unique_ptr<SharedMemoryInternal> p): p(std::move(p)) {}
SharedMemory::SharedMemory(SharedMemory&&) noexcept = default;

} // namespace ipclib
