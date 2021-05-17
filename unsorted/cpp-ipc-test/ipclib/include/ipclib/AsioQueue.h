// Non-blocking ASIO wrapper for Queue

#pragma once

#include <asio.hpp>
#include "ipclib/Queue.h"

namespace ipclib
{

class AsioQueueInternal;


class AsioQueueProducer {
public:
	void async_send(asio::const_buffer buffer, std::function<void(std::error_code error)> writer);
	
	AsioQueueProducer(asio::io_context& io, QueueProducer q);
	~AsioQueueProducer();
	
	AsioQueueProducer(const AsioQueueProducer&) = delete;
    AsioQueueProducer(AsioQueueProducer&&);
	
private:
	QueueProducer q;
	std::unique_ptr<AsioQueueInternal> p;
};


class AsioQueueConsumer {
public:
	// TODO: don't silently fail if buffer is too small
	void async_receive(asio::mutable_buffer buffer, std::function<void(std::error_code error, size_t has_read)> reader);
	
	AsioQueueConsumer(asio::io_context& io, QueueConsumer q);
	~AsioQueueConsumer();
	
	AsioQueueConsumer(const AsioQueueConsumer&) = delete;
    AsioQueueConsumer(AsioQueueConsumer&&);
	
private:
	QueueConsumer q;
	std::unique_ptr<AsioQueueInternal> p;
};

} // namespace ipclib
