#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef int (*goCallback)(char*, int);

typedef struct {
    goCallback callback;
    char* buffer;
    size_t size;
} output;

void append(output* out, const char* format, ...);
int Execute(char* argsBuffer, uint32_t bufferSize, goCallback callback);
