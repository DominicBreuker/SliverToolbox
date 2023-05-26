#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "portscan.h"

#pragma comment(lib, "Ws2_32.lib")


typedef struct {
    int host_count;
    char **hostnames;
    int port_count;
    int *ports;
    int delay;
} portscan_args;


int check_ports(const char **hostnames, int host_count, const int *ports, int port_count, int delay, output *out);
int scan_port(const char *hostname, int port, output *out);

static int parse_arguments(portscan_args *args, output *out, char **cmd_args);
static char* find_arg(const char *prefix, char **args, const char *default_value);
static char** split_string(const char *str, char delimiter, int *count);


int portscan(output* out, char** args) {
    portscan_args parsed_args;

    int parse_result = parse_arguments(&parsed_args, out, args);
    if (parse_result != 0) {
        return parse_result;
    }

    // Call check_ports function
    int result = check_ports((const char **)parsed_args.hostnames, parsed_args.host_count, parsed_args.ports, parsed_args.port_count, parsed_args.delay, out);

    // Free allocated memory
    for (int i = 0; i < parsed_args.host_count; i++) {
        free(parsed_args.hostnames[i]);
    }
    free(parsed_args.hostnames);
    free(parsed_args.ports);

    return result;
}


int check_ports(const char **hostnames, int host_count, const int *ports, int port_count, int delay, output *out) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);

    if (result != 0) {
        append(out, "WSAStartup failed with error: %d\n", result);
        return 1;
    }

    int i, j;
    int scan_result;
    int has_error = 0;
    for (i = 0; i < port_count; i++) {
        for (j = 0; j < host_count; j++) {
            scan_result = scan_port(hostnames[j], ports[i], out);
            if (scan_result != 0) {
                has_error = 1;
            }
            Sleep(delay);
        }
    }

    WSACleanup();

    return has_error ? 1 : 0;
}


int scan_port(const char *hostname, int port, output *out) {
    struct addrinfo *result_addr = NULL, *ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int result = getaddrinfo(hostname, port_str, &hints, &result_addr);
    if (result != 0) {
        append(out, "getaddrinfo failed with error: %d for host: %s\n", result, hostname);
        return -1;
    }

    SOCKET ConnectSocket = INVALID_SOCKET;
    ptr = result_addr;
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        append(out, "Error at socket() for host %s: %ld\n", hostname, WSAGetLastError());
        freeaddrinfo(result_addr);
        return -1;
    }

    int connection_timeout = 500;  // Timeout in milliseconds

    unsigned long mode = 1;
    ioctlsocket(ConnectSocket, FIONBIO, &mode);

    result = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (result == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set write_fds, except_fds;
            FD_ZERO(&write_fds);
            FD_ZERO(&except_fds);
            FD_SET(ConnectSocket, &write_fds);
            FD_SET(ConnectSocket, &except_fds);

            struct timeval timeout;
            timeout.tv_sec = connection_timeout / 1000;
            timeout.tv_usec = (connection_timeout % 1000) * 1000;

            result = select(0, NULL, &write_fds, &except_fds, &timeout);
            if (result == 0 || FD_ISSET(ConnectSocket, &except_fds)) {
				append(out, "[Portscan] %s:%d closed (timeout)\n", hostname, port);
            } else {
				append(out, "[Portscan] %s:%d open\n", hostname, port);
            }
        } else {
			append(out, "[Portscan] %s:%d closed\n", hostname, port);
        }
    } else {
		append(out, "[Portscan] %s:%d open\n", hostname, port);
    }

    // Set the socket back to blocking mode
    mode = 0;
    ioctlsocket(ConnectSocket, FIONBIO, &mode);

    closesocket(ConnectSocket);
    freeaddrinfo(result_addr);

    return 0;
}

// ######### Arguments parsing #########

static int parse_arguments(portscan_args *args, output *out, char **cmd_args) {
    char *hosts_arg = find_arg("hosts:", cmd_args, NULL);
    char *ports_arg = find_arg("ports:", cmd_args, NULL);
    char *delay_arg = find_arg("delay:", cmd_args, "500");

    if (!hosts_arg || !ports_arg || !delay_arg) {
        append(out, "Error: Missing required arguments (hosts, ports, delay)\n");
        return 1;
    }

    // Parse hosts
    char **hostnames = split_string(hosts_arg, ',', &args->host_count);
    if (!hostnames) {
        append(out, "Error: Failed to parse hosts\n");
        return 1;
    }

    // Parse ports
    char **ports_str = split_string(ports_arg, ',', &args->port_count);
    if (!ports_str) {
        append(out, "Error: Failed to parse ports\n");
        for (int i = 0; i < args->host_count; i++) {
            free(hostnames[i]);
        }
        free(hostnames);
        return 1;
    }

    int *ports = (int*)malloc(args->port_count * sizeof(int));
    if (!ports) {
        append(out, "Error: Memory allocation failed for ports\n");
        for (int i = 0; i < args->host_count; i++) {
            free(hostnames[i]);
        }
        for (int i = 0; i < args->port_count; i++) {
            free(ports_str[i]);
        }
        free(hostnames);
        free(ports_str);
        return 1;
    }

    for (int i = 0; i < args->port_count; i++) {
        int port = atoi(ports_str[i]);
        if (port < 1 || port > 65535) {
            append(out, "Error: Invalid port number %d\n", port);
            for (int j = 0; j < args->host_count; j++) {
                free(hostnames[j]);
            }
            for (int j = 0; i < args->port_count; j++) {
                free(ports_str[j]);
            }
            free(hostnames);
            free(ports_str);
            free(ports);
            return 1;
        }
        ports[i] = port;
    }

    // Parse delay
    int delay = atoi(delay_arg);
    if (delay <= 0) {
        append(out, "Error: Invalid delay value %d\n", delay);
        for (int i = 0; i < args->host_count; i++) {
            free(hostnames[i]);
        }
        for (int i = 0; i < args->port_count; i++) {
            free(ports_str[i]);
        }
        free(hostnames);
        free(ports_str);
        free(ports);
        return 1;
    }

    args->hostnames = hostnames;
    args->ports = ports;
    args->delay = delay;

    for (int i = 0; i < args->port_count; i++) {
        free(ports_str[i]);
    }
    free(ports_str);

    return 0;
}

// Generic function to find and parse arguments
static char* find_arg(const char *prefix, char **args, const char *default_value) {
    while (*args) {
        if (strncmp(*args, prefix, strlen(prefix)) == 0) {
            return *args + strlen(prefix);
        }
        args++;
    }
    return (char *)default_value;
}

// Function to split a string using a delimiter and return the resulting array and count
static char** split_string(const char *str, char delimiter, int *count) {
    *count = 0;
    int capacity = 10;
    char **result = (char**)malloc(capacity * sizeof(char*));
    if (!result) {
        return NULL;
    }

    const char *start = str;
    const char *end;
    while (1) {
		end = strchr(start, delimiter);
        if (!end) {
            break;
        }
		
        size_t len = (size_t)(end - start);
        result[*count] = (char*)malloc(len + 1);
        if (!result[*count]) {
            for (int i = 0; i < *count; i++) {
                free(result[i]);
            }
            free(result);
            return NULL;
        }
        strncpy_s(result[*count], len + 1, start, len);
        result[*count][len] = '\0';
        (*count)++;
        if (*count == capacity) {
            capacity *= 2;
            char** resized_result = (char**)realloc(result, capacity * sizeof(char*));
            if (!resized_result) {
                for (int i = 0; i < *count; i++) {
                    free(result[i]);
                }
                free(result);
                return NULL;
            }
            result = resized_result;
        }
        start = end + 1;
    }

    size_t len = strlen(start);
    result[*count] = (char*)malloc(len + 1);
    if (!result[*count]) {
        for (int i = 0; i < *count; i++) {
            free(result[i]);
        }
        free(result);
        return NULL;
    }
    strcpy_s(result[*count], len + 1, start);
    (*count)++;

    return result;
}