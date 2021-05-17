#include "ipclib/AsioQueue.h"

#include <cstdio>
#include <condition_variable>
#include <queue>
#include <thread>

namespace ipclib
{

class AsioQueueInternal {
public:
	AsioQueueInternal(asio::io_context& io): io(io) {
		io.get_executor().on_work_started();
		thr = std::thread([this]{
			auto pred = [&] {return thr_end || !fs.empty();};
			while (true) {
				std::unique_lock<std::mutex> lock(mut);
				cond.wait(lock, pred);
				if (thr_end) {
					while (!fs.empty()) {
						fs.front()(false);
						fs.pop();
					}
					
					break;
				}
				else {
					auto f = std::move(fs.front());
					fs.pop();
					lock.unlock();
					
					f(true);
				}
			}
		});
	}
	~AsioQueueInternal() {
		{	std::unique_lock<std::mutex> lock(mut);
			thr_end = true;
		}
		thr.join();
		io.get_executor().on_work_finished();
	}
	void add(std::function<void(bool)> f) {
		std::unique_lock<std::mutex> lock(mut);
		fs.emplace(std::move(f));
		cond.notify_one();
	}
	void asio_call(std::function<void()> f) {
		asio::dispatch(io, std::move(f));
	}
	
private:
	asio::io_context& io;
	std::thread thr;
	bool thr_end = false;
	std::queue<std::function<void(bool)>> fs;
	std::mutex mut;
	std::condition_variable cond;
};


void AsioQueueProducer::async_send(asio::const_buffer buffer, std::function<void(std::error_code error)> writer) {
	p->add([this, buffer = std::move(buffer), writer = std::move(writer)](bool ok) {
		std::error_code error;
		if (ok) {
			try {
				q.write_message([&buffer](auto mem){
					std::memcpy(mem, buffer.data(), buffer.size());
				}, buffer.size());
			}
			catch (std::system_error& e) {
				error = e.code();
			}
			catch (std::exception& e) {
				fprintf(stderr, "AsioQueueProducer::async_send() exception occured: %s\n", e.what());
				error = std::make_error_code(std::errc::io_error);
			}
		}
		else {
			error = std::make_error_code(std::errc::operation_canceled);
		}
		p->asio_call([writer = std::move(writer), error] {
			writer(error);
		});
	});
}
AsioQueueProducer::AsioQueueProducer(asio::io_context& io, QueueProducer q): q(std::move(q)), p(std::make_unique<AsioQueueInternal>(io)) {}
AsioQueueProducer::~AsioQueueProducer() = default;
AsioQueueProducer::AsioQueueProducer(AsioQueueProducer&&) = default;


void AsioQueueConsumer::async_receive(asio::mutable_buffer buffer, std::function<void(std::error_code error, size_t has_read)> reader) {
	p->add([this, buffer = std::move(buffer), reader = std::move(reader)](bool ok) {
		std::error_code error;
		size_t size = 0;
		if (ok) {
			try {
				auto ret = q.read_message([&buffer, &size](auto mem, auto mem_size){
					if (size <= buffer.size()) {
						size = mem_size;
					}
					else {
						fprintf(stderr, "AsioQueueConsumer::async_receive() buffer is too small, read only %d bytes of %d\n", static_cast<int>(buffer.size()), static_cast<int>(mem_size));
						size = buffer.size();
					}
					std::memcpy(buffer.data(), mem, size);
				});
				switch (ret) {
					case QueueConsumer::ReadOk:
						break;
					case QueueConsumer::ReadCancelled:
					case QueueConsumer::ReadDestroyed:
						error = std::make_error_code(std::errc::operation_canceled);
						break;
					case QueueConsumer::ReadNoProducersLeft:
						error = std::make_error_code(std::errc::broken_pipe);
						break;
				}
			}
			catch (std::system_error& e) {
				error = e.code();
			}
			catch (std::exception& e) {
				fprintf(stderr, "AsioQueueConsumer::async_receive() exception occured: %s\n", e.what());
				error = std::make_error_code(std::errc::io_error);
			}
		}
		else {
			error = std::make_error_code(std::errc::operation_canceled);
		}
		p->asio_call([reader = std::move(reader), error, size] {
			reader(error, size);
		});
	});
}
AsioQueueConsumer::AsioQueueConsumer(asio::io_context& io, QueueConsumer q): q(std::move(q)), p(std::make_unique<AsioQueueInternal>(io)) {}
AsioQueueConsumer::~AsioQueueConsumer() {
	q.cancel_read();
}
AsioQueueConsumer::AsioQueueConsumer(AsioQueueConsumer&&) = default;

} // namespace ipclib
