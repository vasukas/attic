// Blocking unbounded multi-producer multi-consumer interprocess queue
// All functions can throw unless explicitly marked noexcept

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace ipclib {

class QueueInternal;


/// Removes shm object; existing producers/consumers will continue to work, but name is freed
void remove_queue(const std::string& name) noexcept;


class QueueProducer {
public:
    /// Calls function with internally-allocated memory of specified size
    void write_message(std::function<void(void *mem)> writer, size_t size);

    static QueueProducer create(const std::string& name, bool allow_existing = false);
    static QueueProducer open(const std::string& name);

    /// Destroys underlying object if no other users remain
    ~QueueProducer() noexcept;

    QueueProducer(const QueueProducer&) = delete;
    QueueProducer(QueueProducer&&) noexcept;

private:
    std::unique_ptr<QueueInternal> p;
    QueueProducer(std::unique_ptr<QueueInternal> p);
};


class QueueConsumer {
public:
    enum ReadRet {
        ReadOk,
        ReadCancelled,
        ReadDestroyed, ///< This object no longer exists
        ReadNoProducersLeft
    };

    static std::error_code get_error_code(ReadRet ret);

    /// Calls function when new message is received
    ReadRet read_message(std::function<void(const void *mem, size_t size)> reader);

    /// Cancels waiting read with ReadCancelled. Can be safely called from another thread
    void cancel_read();

    static QueueConsumer create(const std::string& name, bool allow_existing = false);
    static QueueConsumer open(const std::string& name);

    /// Destroys underlying object if no other users remain.
    /// Cancels waiting reads with ReadDestroyed
    ~QueueConsumer() noexcept;

    QueueConsumer(const QueueConsumer&) = delete;
    QueueConsumer(QueueConsumer&&) noexcept;

private:
    std::unique_ptr<QueueInternal> p;
    uint64_t uid;
    uint64_t last_cancel_all;

    QueueConsumer(std::unique_ptr<QueueInternal> p);
};

} // namespace ipclib
