#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>

#include "bootleg_stdio.h"

int open_file(int fileNo) {
	char msg[64];
	snprintf(msg, sizeof(msg), "Enter (%d) output file path: ", fileNo);
	if (write(STDIN_FILENO, msg, strlen(msg)) == -1) {
		blg_perrorf("can't write to stdout (1)");
	}

	char path[PATH_MAX];
	size_t nRead = read(STDIN_FILENO, &path, sizeof(path) - 1);
	if (nRead == -1) {
		blg_perrorf("can't read (%d) output file path\n", fileNo);
	}
	// Remove line separator if it's present
	if (nRead > 0 && path[nRead - 1] == '\n') {
		path[nRead - 1] = '\0';
	}

	int file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR);
	if (file == -1) {
		blg_perrorf("can't open (%d) output file for writing\n", fileNo);
	}

	return file;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		blg_perrorf("usage: %s <client program>\n", argv[0]);
	}

	char* client = argv[1];
	const int nChildren = 2;

	int files[nChildren];
	for (int i = 0; i != nChildren; ++i) {
		files[i] = open_file(i + 1);
	}

	int pipes[nChildren][2];
	for (int i = 0; i != nChildren; ++i) {
		if (pipe(pipes[i]) == -1) {
			blg_perrorf("can't create pipe (%d)\n", i);
		}
	}

	for (int i = 0; i != nChildren; ++i) {
		pid_t child = fork();
		if (child == -1) {
			blg_perrorf("can't fork on child %d\n", i);
		}

		if (child == 0) {
			if (dup2(pipes[i][STDIN_FILENO], STDIN_FILENO) == -1) {
				blg_perrorf("can't redirect child's stdin to pipe\n");
			}
			if (dup2(files[i], STDOUT_FILENO) == -1) {
				blg_perrorf("can't redirect child's stdout to file\n");
			}
			close(pipes[i][STDOUT_FILENO]);

			if (execl(/* path */ client, /* argv[0] */ client, (char*)NULL) == -1) {
				blg_perrorf("can't exec into child\n");
			}
		}
	}

	char* buffer = NULL;
	ssize_t nRead;

	for (int i = 0; (nRead = blg_getline(&buffer, 0, STDIN_FILENO)) != 0; ++i) {
		if (nRead == -1) {
			free(buffer);
			blg_perrorf("can't read from stdin\n");
		}

		int* pipe = pipes[i % nChildren];
		if (write(pipe[STDOUT_FILENO], buffer, nRead) == -1) {
			free(buffer);
			blg_perrorf("can't write to pipe\n");
		}

		free(buffer);
		buffer = NULL;
	}

	// Wait for all children to exit.
	while (wait(NULL) > 0)
		;
}
