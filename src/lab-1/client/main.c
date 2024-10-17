#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bootleg_stdio.h"

inline static bool is_vowel(char c) {
	bool lowercase = c == 'a' || c == 'e' || c == 'o' || c == 'i' || c == 'u';
	bool uppercase = c == 'A' || c == 'E' || c == 'O' || c == 'I' || c == 'U';
	return lowercase || uppercase;
}

bool remove_vowels(const char* in, size_t inLength, char** out, size_t* outLength) {
	if (in == NULL || inLength == 0 || out == NULL || outLength == NULL) {
		return false;
	}

	*out = malloc((inLength + 1) * sizeof(char));
	if (*out == NULL) return false;

	// Copy chars from |in|, excluding vowels.
	size_t j = 0;
	for (char* ptr = (char*)in; *ptr != '\0'; ++ptr) {
		if (!is_vowel(*ptr)) {
			(*out)[j++] = *ptr;
		}
	}

	// Null-terminate the string.
	(*out)[j] = '\0';
	*outLength = j;

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
