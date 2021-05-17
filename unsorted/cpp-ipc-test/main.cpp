#include <cstdio>
#include <cstring>
#include <thread>

#include "ipclib/AsioQueue.h"
#include "ipclib/SharedMemory.h"

void sleep(int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void test_SharedMemory(bool is_writer) {
	if (is_writer) {
		auto shm = ipclib::SharedMemory::create("test");
		
		shm.resize(65536);
		std::memcpy(shm.write_lock().data(), "OH HAI", 7);
		printf("Write OK\n");
		
		sleep(2000);
		shm.resize(1);
		printf("W. shm size is %d\n", int(shm.size()));
	}
	else {
		auto shm = ipclib::SharedMemory::open("test");
		
		printf("shm size is %d\n", int(shm.size()));
		printf("%s\n", shm.read_lock().data());
		
		sleep(2000);
		printf("1. shm size is %d\n", int(shm.size()));
		shm.write_lock().data()[65535] = 42;
		shm.update_size();
		printf("2. shm size is %d\n", int(shm.size()));
	}
}

void test_AsioQueue(bool is_writer) {
	asio::io_context io;
	uint8_t mem;
	
	if (is_writer) {
		static auto q = ipclib::AsioQueueProducer(io, ipclib::QueueProducer::create("test"));
		
		std::function<void()> f = [&]{
			q.async_send(asio::const_buffer(&mem, 1), [&](auto err){
				if (err) {
					printf("SEND error: %s\n", err.message().c_str());
				}
				else {
					printf("sent %d\n", mem);
					if (++mem < 2) {
						sleep(500);
						f();
					}
				}
			});
		};
		
		mem = 0;
		f();
	}
	else {
		static auto q = ipclib::AsioQueueConsumer(io, ipclib::QueueConsumer::open("test"));
		
		std::function<void()> f = [&]{
			q.async_receive(asio::buffer(&mem, 1), [&](auto err, auto size) {
				if (err) {
					printf("RECEIVE error: %s\n", err.message().c_str());
				}
				else {
					if (size != 1) {
						printf("RECEIVE INVALID SIZE: %d\n", int(size));
					}
					else {
						printf("received %d\n", mem);
						f();
					}
				}
			});
		};
		
		f();
	}
	
	std::thread([&] {
		io.run();
		printf("%s io finished\n", is_writer ? "WRITER" : "READER");
	}).join();
}

int main(int argc, char *argv[]) {
    (void) argv;

	bool is_writer = (argc == 1);
	
	if (is_writer) {
		ipclib::SharedMemory::remove("test"); // implementation detail, but works for all types ^^
	}
	printf("%s\n", is_writer ? "WRITER" : "READER");
	
    try {
        //test_SharedMemory(is_writer);
		test_AsioQueue(is_writer);
		
		printf("%s FINISHED\n", is_writer ? "WRITER" : "READER");
    }
    catch (std::exception& e) {
        printf("main() exception: %s\n", e.what());
    }
}
