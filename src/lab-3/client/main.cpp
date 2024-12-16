#include <format>

// Shared Memory
#include <fcntl.h>
#include <sys/mman.h>

// Semaphore
#include <semaphore.h>

#include "lab.h"
#include "posix_buf.h"

int main(int argc, char* argv[]) {
	if (argc != 4) {
		perr << std::format(
		    "usage: {} <shared memory path> <p2c semaphore path> <c2p semaphore path>\n",
		    argv[0]);
		return 1;
	}

	const pid_t pid = getpid();

	sem_t* p2c = sem_open(argv[2], 0);
	if (!p2c) {
		perr << "can't open parent-to-child semaphore" << std::endl;
		return 2;
	}

	sem_t* c2p = sem_open(argv[3], 0);
	if (!c2p) {
		perr << "can't open child-to-parent semaphore" << std::endl;
		return 3;
	}

	int shm = shm_open(argv[1], O_RDWR, S_IRUSR | S_IWUSR);
	if (shm == -1) {
		perr << "can't open shared memory" << std::endl;
		return 4;
	}

	char* shmBuf = static_cast<char*>(
	    mmap(nullptr, MAX_LINE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0));
	if (shmBuf == MAP_FAILED) {
		perr << "can't map shared memory" << std::endl;
		return 5;
	}

	while (true) {
		// Wait for a signal from parent.
		sem_wait(p2c);

		// Exit signal.
		if (*shmBuf == '\0') {
			break;
		}

		std::string input(shmBuf);

		// Erase vowels from the input.
		std::erase_if(input, [](char c) {
			bool lowercase = c == 'a' || c == 'e' || c == 'o' || c == 'i' || c == 'u';
			bool uppercase = c == 'A' || c == 'E' || c == 'O' || c == 'I' || c == 'U';

			return lowercase || uppercase;
		});

		// Write back the result.
		std::strncpy(shmBuf, input.c_str(), MAX_LINE);

		// Notify parent of the result.
		sem_post(c2p);
	}

	munmap(shmBuf, MAX_LINE);
	sem_close(p2c);
	sem_close(c2p);
}