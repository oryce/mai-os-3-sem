#pragma once

#include <unistd.h>

ssize_t blg_getline(char** lineptr, size_t n, int stream);

void blg_perrorf(const char* fmt, ...);

void blg_printf(const char* fmt, ...);