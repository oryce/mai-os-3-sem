#include <algorithm>
#include <format>
#include <string>
#include <vector>

// Shared Memory
#include <fcntl.h>
#include <sys/mman.h>

// Semaphores
#include <semaphore.h>

#include "lab.h"
#include "posix_buf.h"

static const size_t CHILDREN = 2;

class SharedMemory {
	/** Path to the shared memory file. */
	std::string path_;
	/** File descriptor of the shared memory object. */
	int fd_;
	/** Mapped buffer. */
	char* buf_;
	/** Size of the mapped buffer. */
	size_t size_;

   public:
	SharedMemory() : fd_(-1), buf_(nullptr), size_(0) {}

	// copying this shouldn't be possible
	SharedMemory(const SharedMemory&) = delete;
	SharedMemory& operator=(const SharedMemory&) = delete;

	explicit SharedMemory(const std::string& path, size_t size)
	    : path_(path), size_(size) {
		fd_ = shm_open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd_ == -1) {
			throw std::runtime_error("can't create shared memory object");
		}

		if (ftruncate(fd_, static_cast<off_t>(size)) == -1) {
			throw std::runtime_error("can't resize shared memory");
		}

		buf_ = static_cast<char*>(
		    mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
		if (buf_ == MAP_FAILED) {
			throw std::runtime_error("can't map shared memory");
		}
	}

	~SharedMemory() {
		munmap(buf_, size_);
		shm_unlink(path_.c_str());
	}

	const std::string& path() const { return path_; }

	char* buf() { return buf_; }

	size_t size() const { return size_; }
};

struct Child {
	/** Child process ID. */
	pid_t pid_;
	/** Output file descriptor. */
	int output_fd_;
	/** Path to the P2C semaphore. */
	std::string lock_path_p2c_;
	/** Semaphore for parent-to-child communication. */
	sem_t* lock_p2c_;
	/** Path to the C2P semaphore. */
	std::string lock_path_c2p_;
	/** Semaphore for child-to-parent communication. */
	sem_t* lock_c2p_;

	Child() : pid_(-1), output_fd_(-1), lock_p2c_(nullptr), lock_c2p_(nullptr) {}

	~Child() {
		close(output_fd_);
		sem_close(lock_p2c_);
		sem_unlink(lock_path_p2c_.c_str());
		sem_close(lock_c2p_);
		sem_unlink(lock_path_c2p_.c_str());
	}
};

int main(int argc, char* argv[]) {
	if (argc != 2) {
		perr << std::format("usage: {} <client program>", argv[0]);
		return 1;
	}

	char* childPath = argv[1];

	// Initialize and map shared memory.
	SharedMemory shm(std::format("/line_buffer_{}", getpid()), MAX_LINE);

	std::vector<Child> children(CHILDREN);

	// Prompt the user for filenames and open them.
	for (size_t i = 0; i != CHILDREN; ++i) {
		auto& child = children[i];

		pout << "Enter filename of file " << (i + 1) << ": " << std::endl;

		std::string filename;
		pin >> filename;

		int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
		if (fd == -1) {
			perr << "can't open the file for writing" << std::endl;
			return 5;
		}

		child.output_fd_ = fd;
	}

	// Create semaphores.
	for (size_t i = 0; i != CHILDREN; ++i) {
		auto& child = children[i];

		std::string lockPathP2c = std::format("/lock_{}_p2c", i);
		child.lock_path_p2c_ = lockPathP2c;

		sem_t* lockP2c = sem_open(lockPathP2c.c_str(), O_CREAT, S_IRUSR | S_IWUSR, 0);
		if (!lockP2c) {
			perr << "can't create P2C semaphore" << std::endl;
			return 6;
		}
		child.lock_p2c_ = lockP2c;

		std::string lockPathC2p = std::format("/lock_{}_c2p", i);
		child.lock_path_c2p_ = lockPathC2p;

		sem_t* lockC2p = sem_open(lockPathC2p.c_str(), O_CREAT, S_IRUSR | S_IWUSR, 0);
		if (!lockC2p) {
			perr << "can't create C2P semaphore" << std::endl;
			return 7;
		}
		child.lock_c2p_ = lockC2p;
	}

	// Start children.
	for (auto& child : children) {
		pid_t pid = fork();
		if (pid == -1) {
			perr << "can't fork() to create a child process" << std::endl;
			return 7;
		}
		if (pid == 0) {
			if (execl(/* path */ childPath, /* argv[0] */ childPath,
			          /* argv[1] */ shm.path().c_str(),
			          /* argv[2] */ child.lock_path_p2c_.c_str(),
			          /* argv[3] */ child.lock_path_c2p_.c_str(), nullptr) == -1) {
				perr << "can't start child process" << std::endl;
				return 8;
			}
		} else {
			child.pid_ = pid;
		}
	}

	// Read lines from standard input and write them to shared memory. Notify children
	// that the buffer changed. Wait for a response and write it to the output file.
	size_t lines = 0;

	std::string line;
	while (std::getline(pin, line)) {
		auto& child = children[lines % children.size()];

		if (line.empty()) {
			continue;
		}

		if (line.length() > MAX_LINE) {
			perr << "Input line is too large. Please input a line containing less "
			        "characters."
			     << std::endl;
			continue;
		}

		// Copy the null-terminated string to shared memory.
		std::strncpy(shm.buf(), line.c_str(), shm.size());

		// Notify the child process that the data is ready.
		sem_post(child.lock_p2c_);
		// Wait for a response from the child process.
		sem_wait(child.lock_c2p_);

		// Read response from shared memory and write it to the file.
		std::string response(shm.buf());
		response.push_back('\n');

		size_t written = write(child.output_fd_, response.data(), response.length());
		if (written != response.length()) {
			perr << "can't write to output file" << std::endl;
		}

		++lines;
	}

	// EOF received -- zero the shared memory and notify all children to exit.
	std::fill(shm.buf(), shm.buf() + shm.size(), '\0');

	for (auto& child : children) {
		sem_post(child.lock_p2c_);
	}

	while (wait(nullptr) > 0)
		;
}
