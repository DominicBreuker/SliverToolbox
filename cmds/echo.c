#include "echo.h"

int echo(output* out, char** args) {
    for (size_t i = 0; args[i] != NULL; ++i) {
        append(out, "%s%s", args[i], args[i + 1] != NULL ? " " : "");
    }
    return 0;
}