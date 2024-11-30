#include "bootleg_stdio.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ssize_t blg_getline(char** lineptr, size_t n, int stream) {
	if (lineptr == NULL || (*lineptr != NULL && n == 0)) {
		errno = EINVAL;
		return -1;
	}

	if (*lineptr == NULL) {
		n = 16;

		// Allocate one more char for the line separator.
		*lineptr = (char*)malloc((n + 1) * sizeof(char));
		if (*lineptr == NULL) {
			errno = ENOMEM;
			return -1;
		}
	}

	char buffer[256];
	ssize_t nRead;
	ssize_t nWritten = 0;

	while ((nRead = read(stream, &buffer, sizeof(buffer))) != 0) {
		if (nRead == -1) {
			return -1;
		}

		for (int i = 0; i != nRead; ++i) {
			// Current buffer is too small to hold the result.
			if (nWritten == n) {
				n *= 2;

				char* newBuffer = (char*)realloc(*lineptr, (n + 1) * sizeof(char));
				if (newBuffer == NULL) {
					errno = ENOMEM;
					return -1;
				}

				*lineptr = newBuffer;
			}

			(*lineptr)[nWritten] = buffer[i];
			++nWritten;

			if (buffer[i] == '\n') {
				(*lineptr)[nWritten] = '\0';
				return nWritten;
			}
		}
	}

	(*lineptr)[nWritten] = '\0';
	return nWritten;
}

void blg_perrorf(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char msg[256];
	vsnprintf(msg, sizeof(msg), fmt, args);

	va_end(args);

	write(STDERR_FILENO, msg, strlen(msg));
	exit(EXIT_FAILURE);
}
