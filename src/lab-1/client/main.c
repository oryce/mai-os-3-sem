#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bootleg_stdio.h"

inline static bool is_vowel(char c) {
	return c == 'a' || c == 'e' || c == 'o' || c == 'i' || c == 'u' || c == 'A' || c == 'E' || c == 'O' || c == 'I' ||
	       c == 'U';
}

bool remove_vowels(const char* in, size_t length, char** out, size_t* newLength) {
	if (in == NULL || length == 0 || out == NULL) return false;

	*out = malloc((length + 1) * sizeof(char));
	if (*out == NULL) return false;

	char* ptr = (char*)in;
	size_t j = 0;

	while (*ptr != '\0') {
		if (!is_vowel(*ptr)) {
			(*out)[j] = *ptr;
			++j;
		}
		++ptr;
	}

	(*out)[j] = '\0';
	*newLength = j;

	return true;
}

int main(int argc, char** argv) {
	char* buffer = NULL;
	char* modified = NULL;
	ssize_t nRead;

	while ((nRead = blg_getline(&buffer, 0, STDIN_FILENO))) {
		if (nRead == -1) {
			free(buffer);
			blg_perrorf("[child] can't read from stdin\n");
		}

		// Exit condition
		if (*buffer == '\n') {
			free(buffer);
			free(modified);
			exit(EXIT_SUCCESS);
		}

		size_t newLength;
		if (!remove_vowels(buffer, nRead, &modified, &newLength)) {
			free(buffer);
			blg_perrorf("[child] can't modify the buffer\n");
		}

		if (write(STDOUT_FILENO, modified, newLength) == -1) {
			free(buffer);
			free(modified);
			blg_perrorf("[child] can't write to stdout\n");
		}

		free(buffer);
		free(modified);
		buffer = NULL;
		modified = NULL;
	}
}
