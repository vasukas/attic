// Multiple-readers/single-writer interprocess memory
// All functions can throw unless explicitly marked noexcept

#pragma once

#include <memory>
#include <string>

namespace ipclib
{

class SharedMemoryInternal;
class SharedMemoryInternalRead;
class SharedMemoryInternalWrite;


class SharedMemoryReadLock {
public:
    const uint8_t *data() const noexcept;
    size_t size() const noexcept;
	
	SharedMemoryReadLock(SharedMemoryInternal& p);
    ~SharedMemoryReadLock() noexcept;

    SharedMemoryReadLock(const SharedMemoryReadLock&) = delete;
    SharedMemoryReadLock(SharedMemoryReadLock&&) noexcept;
	SharedMemoryReadLock& operator=(SharedMemoryReadLock&&);

private:
	std::unique_ptr<SharedMemoryInternalRead> p;
};


class SharedMemoryWriteLock {
public:
    uint8_t *data() const noexcept;
    size_t size() const noexcept;
	
	SharedMemoryWriteLock(SharedMemoryInternal& p);
    ~SharedMemoryWriteLock() noexcept;

    SharedMemoryWriteLock(const SharedMemoryWriteLock&) = delete;
    SharedMemoryWriteLock(SharedMemoryWriteLock&&) noexcept;
	SharedMemoryWriteLock& operator=(SharedMemoryWriteLock&&);

private:
	std::unique_ptr<SharedMemoryInternalWrite> p;
};


class SharedMemory {
public:
	// Name must follow same rules as C++ identifiers - beginning with letter, only alphanumericals and '_' symbols are allowed.
	//
	// Internally shared memory is represented by two objects - shm object and mapped region.
	// Shm is an actual shared memory, mapped region is a mapping of that memory
	// to the address space of a process. They may be resized independently.
	//
	// BEWARE: if shm is downsized, access to mapping in any of the processes will
	// cause segmentation fault!
	
	/// Throws if already exists
    static SharedMemory create(const std::string& name, bool allow_existing = false);
	
	/// Throws if doesn't exist
    static SharedMemory open(const std::string& name);
	
	/// Removes shm object; existing mappings will continue to work, but name is freed
	static void remove(const std::string& name) noexcept;
	
	/// Doesn't remove object - it will exist until reboot or explicit remove
    ~SharedMemory() noexcept;
	
	/// Returns size of mapped region
    size_t size() const noexcept;
	
	/// Resizes both shm and mapped region
    void resize(size_t new_size);

	/// Resizes mapped region to shm size
	size_t update_size();
	
    SharedMemoryReadLock read_lock(); ///< Blocks until available
    SharedMemoryWriteLock write_lock(); ///< Blocks until available

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) noexcept;

private:
    std::unique_ptr<SharedMemoryInternal> p;
    SharedMemory(std::unique_ptr<SharedMemoryInternal> p);
};

} // namespace ipclib
