#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

typedef int (*ExecuteFunc)(char*, uint32_t, int (*)(char*, int));

int goCallback(char* buffer, int size) {
    printf("Callback:\n%s\n", buffer);
    return size;
}

int main(int argc, char* argv[]) {
    HMODULE hDLL = LoadLibrary("SliverToolbox.dll");
    if (!hDLL) {
        printf("Error: unable to load the DLL\n");
        return 1;
    }

    ExecuteFunc Execute = (ExecuteFunc)GetProcAddress(hDLL, "Execute");
    if (!Execute) {
        printf("Error: unable to find the Execute function\n");
        FreeLibrary(hDLL);
        return 1;
    }

    char argsBuffer[4096];
    size_t pos = 0;
    for (int i = 1; i < argc; ++i) {
        size_t len = strlen(argv[i]);
        memcpy(argsBuffer + pos, argv[i], len);
        pos += len;
        if (i != argc - 1) {
            argsBuffer[pos++] = ' ';
        }
    }
    argsBuffer[pos] = '\0';

    int result = Execute(argsBuffer, (uint32_t)pos, goCallback);
    printf("Exit code: %d\n", result);

    FreeLibrary(hDLL);
    return 0;
}
