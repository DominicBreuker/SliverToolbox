#include "SliverToolbox.h"
#include "cmds/echo.h"
#include "cmds/ping.h"
#include "cmds/portscan.h"
#include "cmds/move_winrm.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windows.h>

char** tokenize_args(const char* argsBuffer, uint32_t bufferSize, size_t* arg_count);


int Execute(char* argsBuffer, uint32_t bufferSize, goCallback callback) {
    output out = { callback, NULL, 0 };
	
	//printf("argsBuffer: %s\n", argsBuffer);

    size_t arg_count = 0;
    char** args = tokenize_args(argsBuffer, bufferSize, &arg_count);
    if (!args) {
        append(&out, "Error: argument tokenization failed, memeory allocation error\n");
        return 1;
    }
	
	/*
    for (int i = 0; i < arg_count; ++i) {
        printf("args[%d]: %s\n", i, args[i]);
    }
	*/


    int result = 1;
    if (strcmp(args[0], "echo") == 0) {
        result = echo(&out, &args[1]);
    } else if (strcmp(args[0], "ping") == 0) {
        result = ping(&out);
    } else if (strcmp(args[0], "portscan") == 0) {
        result = portscan(&out, &args[1]);
    } else if (strcmp(args[0], "move-winrm") == 0) {
        result = move_winrm(&out, &args[1]);
    } else {
        append(&out, "Error: unknown command '%s'", args[0]);
    }
	
	
	for (size_t i = 0; i < arg_count; ++i) {
		free(args[i]);
	}
	free(args);
	
    int final_result = callback(out.buffer, (int)out.size);
    free(out.buffer);
    return result == 0 ? final_result : 1;
}


// Helper functions

char** tokenize_args(const char* argsBuffer, uint32_t bufferSize, size_t* arg_count) {
    char** args = (char**)calloc(bufferSize, sizeof(char*));
    if (!args) {
        return NULL;
    }

    int in_single_quote = 0;
    int escaped = 0;
    const char* current_arg_start = argsBuffer;
    const char* current_arg_end = argsBuffer;

    *arg_count = 0;

    for (uint32_t i = 0; i < bufferSize; ++i) {
        char c = argsBuffer[i];
		//printf("[%d] current char: %c\n",i, c);

        if (c == '\\' && !escaped) {
            escaped = 1;
            current_arg_end++;
            continue;
        }

        if (c == '\'' && !escaped) {
            in_single_quote = !in_single_quote;
            if (in_single_quote) {
                current_arg_start++;
            } else {
                current_arg_end++; // Include the closing single quote
            }
        } else if ((c == ' ' && !in_single_quote) || c == '\0') {
            if (current_arg_start != current_arg_end) {
                size_t arg_length = current_arg_end - current_arg_start - (in_single_quote ? 1 : 0);
                args[*arg_count] = (char*)calloc(arg_length + 1, sizeof(char));
                
                const char* src = current_arg_start;
                char* dest = args[*arg_count];
                while (src != current_arg_end) {
					//printf("[%d] dst = src: %c = %c\n",*dest, *src);
                    if (*src == '\\' && *(src + 1) == '\'') {
                        src++; // Skip backslash if it escapes '
                    }
                    *dest = *src;
                    src++;
                    dest++;
                }
                *dest = '\0';

                (*arg_count)++;
            }
            current_arg_start = &argsBuffer[i + 1];
            current_arg_end = current_arg_start;
        } else {
            //current_arg_end++;
			if (!escaped || i < bufferSize - 1) {
				current_arg_end++; // Include the closing single quote
            }
        }

        if (escaped) {
            escaped = 0;
        }

        if (c == '\0') {
            break;
        }
    }

    // Add the last argument if it was not added in the loop
    if (current_arg_start != current_arg_end) {
        size_t arg_length = current_arg_end - current_arg_start;
        args[*arg_count] = (char*)calloc(arg_length + 1, sizeof(char));
        
        const char* src = current_arg_start;
        char* dest = args[*arg_count];
        while (src != current_arg_end) {
            if (*src == '\\' && *(src + 1) == '\'') {
                src++; // Skip the backslash
            }
            *dest = *src;
            src++;
            dest++;
        }
        *dest = '\0';

        (*arg_count)++;
    }

    return args;
}


void append(output* out, const char* format, ...) {
    va_list args;
    va_start(args, format);
    size_t needed = _vscprintf(format, args) + 1;
    out->buffer = (char*)realloc(out->buffer, out->size + needed);
    if (!out->buffer) {
        // Allocation failed, abort
        abort();
    }
    vsprintf_s(out->buffer + out->size, needed, format, args);
    out->size += needed - 1;
    va_end(args);
}

//BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
BOOL APIENTRY DllMain(DWORD ul_reason_for_call) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
