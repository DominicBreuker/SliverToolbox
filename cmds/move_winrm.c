#include <stdio.h>
#include <string.h>
#include <windows.h>
#define WSMAN_API_VERSION_1_0
#include <wsman.h>
#include <wsmandisp.h>

#include "move_winrm.h"


static char* find_arg(const char *prefix, char **args, const char *default_value);
int execute_remote_command(const char *hostname, const char *cmd, const char *username, const char *password, output* out);
int convert_args_to_unicode(const char *input, wchar_t **output);
int execute_remote_command_unicode(const wchar_t *remote_host, const wchar_t *command_line, const wchar_t *w_username, const wchar_t *w_password, output* out);


int move_winrm(output* out, char **args)
{
    char *hostname = find_arg("host:", args, NULL);
    char *cmd = find_arg("cmd:", args, NULL);
    char *username = find_arg("username:", args, NULL);
    char *password = find_arg("password:", args, NULL);

    if (hostname == NULL || cmd == NULL) {
        append(out, "Error: host and cmd arguments are required.\n");
        return 1;
    }
	
	if ((username != NULL && password == NULL) || (username == NULL && password != NULL)) {
        append(out, "Error: If you specify username or password you must specify both\n");
        return 1;
    }

    int result = execute_remote_command(hostname, cmd, username, password, out);

    return result;
}


typedef struct {
    HANDLE evt;
    DWORD errCode;
	output* out;
} CallbackContext;

void cbShellCompletion(
    PVOID context,
	DWORD,
    WSMAN_ERROR *error,
    WSMAN_SHELL_HANDLE,
    WSMAN_COMMAND_HANDLE,
    WSMAN_OPERATION_HANDLE,
    WSMAN_RECEIVE_DATA_RESULT
)
{
	CallbackContext *ctx = (CallbackContext *)context;
	ctx->errCode = 0;
	
    if (error && 0 != error->code)
    {
        append(ctx->out, "cbShellCompletion error\n");
        ctx->errCode = error->code;
        if (error->errorDetail) {
			wprintf(error->errorDetail);
			printf("\n");
		}
    }
    
    SetEvent(ctx->evt);
}

typedef struct {
    HANDLE evt;
    DWORD errCode;
	output* out;
} ReceiveCallbackContext;

void CALLBACK cbReceive(
    PVOID context,
	DWORD,
    WSMAN_ERROR *error,
    WSMAN_SHELL_HANDLE,
    WSMAN_COMMAND_HANDLE,
    WSMAN_OPERATION_HANDLE,
    WSMAN_RECEIVE_DATA_RESULT *data
    )
{
	ReceiveCallbackContext *ctx = (ReceiveCallbackContext *)context;
	ctx->errCode = 0;
	
    if (error && 0 != error->code)
    {
        ctx->errCode = error->code;
        wprintf(error->errorDetail);
    }

    if (data && data->streamData.type == WSMAN_DATA_TYPE_BINARY && data->streamData.binaryData.dataLength)
    {
        for (size_t i = 0; i < data->streamData.binaryData.dataLength; ++i)
        {
            append(ctx->out, "%c", ((unsigned char*)data->streamData.binaryData.data)[i]);
        }
    }
	append(ctx->out, "\n");

    if ((error && 0 != error->code) || (data && data->commandState && wcscmp(data->commandState, WSMAN_COMMAND_STATE_DONE) == 0))
    {
        SetEvent(ctx->evt);
    }
}

int execute_remote_command(const char *hostname, const char *cmd, const char *username, const char *password, output* out) {
    wchar_t *remote_host = NULL;
    wchar_t *command_line = NULL;
    wchar_t *w_username = NULL;
    wchar_t *w_password = NULL;
    int ret = 1;

    if (convert_args_to_unicode(hostname, &remote_host) != 0 ||
        convert_args_to_unicode(cmd, &command_line) != 0) {
        goto cleanup;
    }
	
	if (username != NULL) {
		if (convert_args_to_unicode(username, &w_username) != 0) goto cleanup;
    }
	if (password != NULL) {
		if (convert_args_to_unicode(password, &w_password) != 0) goto cleanup;
    }

    ret = execute_remote_command_unicode(remote_host, command_line, w_username, w_password, out);

cleanup:
    if (remote_host) free(remote_host);
    if (command_line) free(command_line);
    if (w_username) free(w_username);
    if (w_password) free(w_password);

    return ret;
}

int convert_args_to_unicode(const char *input, wchar_t **output) {
	if (input == NULL) {
		return 1;
	}
	
    *output = (wchar_t *)malloc((strlen(input) + 1) * sizeof(wchar_t));
    if (*output == NULL) {
        return 1;
    }
	
    size_t convertedChars = 0;
    errno_t result = mbstowcs_s(&convertedChars, *output, strlen(input) + 1, input, _TRUNCATE);
	if (result != 0) {
        free(*output);
        *output = NULL;
    }
	
    return result;
}

int execute_remote_command_unicode(const wchar_t *remote_host, const wchar_t *command_line, const wchar_t *w_username, const wchar_t *w_password, output* out) {
    const wchar_t *shell_uri = L"http://schemas.microsoft.com/wbem/wsman/1/windows/shell/cmd";
	
	WSMAN_SESSION *session = NULL;
	WSMAN_SHELL *shell = NULL;
	WSMAN_COMMAND *command = NULL;
	WSMAN_API_HANDLE apiHandle = NULL;
	DWORD errCode;
	int ret = 1;

	// Initialize the WinRM client stack
	errCode = WSManInitialize(WSMAN_FLAG_REQUESTED_API_VERSION_1_0, &apiHandle);
	if (NO_ERROR != errCode)
    {
        append(out, "WSManInitialize failed: %d\n", errCode);
        goto cleanup;
    }

    // Set up authentication
	WSMAN_AUTHENTICATION_CREDENTIALS auth_credentials;
	memset(&auth_credentials, 0, sizeof(WSMAN_AUTHENTICATION_CREDENTIALS));
	auth_credentials.authenticationMechanism = WSMAN_FLAG_AUTH_NEGOTIATE;
	if ((w_username != NULL) && (w_password != NULL)) {
		auth_credentials.userAccount.username = w_username;
		auth_credentials.userAccount.password = w_password;
	}

	append(out, "[+] Creating session\n");
	errCode = WSManCreateSession(apiHandle, remote_host, 0, &auth_credentials, NULL, &session);
	if (NO_ERROR != errCode)
    {
        append(out, "WSManCreateSession failed: %d\n", errCode);
        goto cleanup;
    }
	
	WSManSessionOption option = WSMAN_OPTION_DEFAULT_OPERATION_TIMEOUTMS;
    WSMAN_DATA data;
    data.type = WSMAN_DATA_TYPE_DWORD;
    data.number = 60000;
    errCode = WSManSetSessionOption(session, option, &data);
    if (NO_ERROR != errCode) 
    {
        append(out, "WSManSetSessionOption failed: %d\n", errCode);
        goto cleanup;
    }
	
	// Prepare async call
	CallbackContext ctx;
	ctx.out = out;
	ctx.errCode = 0;
    ctx.evt = CreateEvent(0, FALSE, FALSE, NULL);
    if (NULL == ctx.evt)
    {
        errCode = GetLastError();
        append(out, "CreateEvent for shell failed: %d\n", errCode);
        goto cleanup;
    }
	
	WSMAN_SHELL_ASYNC shell_async;
	memset(&shell_async, 0, sizeof(WSMAN_SHELL_ASYNC));
    shell_async.operationContext = &ctx;
    shell_async.completionFunction = (WSMAN_SHELL_COMPLETION_FUNCTION) &cbShellCompletion;
	
	append(out, "[+] Creating shell now\n");
	WSManCreateShell(session, 0, shell_uri, NULL, NULL, NULL, &shell_async, &shell);
	WaitForSingleObject(ctx.evt, INFINITE);
    if (NO_ERROR != ctx.errCode) 
    {
        append(out, "WSManCreateShell failed: %d\n", ctx.errCode);
        goto cleanup;
    }

    // Execute the command
	append(out, "[+] Executing command\n");
    WSManRunShellCommand(shell, 0, command_line, NULL, NULL, &shell_async, &command);
	WaitForSingleObject(ctx.evt, INFINITE);
    if (NO_ERROR != ctx.errCode) 
    {
        append(out, "WSManRunShellCommand failed: %d\n", ctx.errCode);
        goto cleanup;
    }

    // Prepare the asynchronous structure for receiving output
    ReceiveCallbackContext ctxReceive;
	ctxReceive.out = out;
	ctxReceive.errCode = 0;
    ctxReceive.evt = CreateEvent(0, FALSE, FALSE, NULL);
    if (NULL == ctxReceive.evt)
    {
        errCode = GetLastError();
        append(out, "CreateEvent for receive failed: %d\n", errCode);
        goto cleanup;
    }
    WSMAN_SHELL_ASYNC receive_async;
    memset(&receive_async, 0, sizeof(WSMAN_SHELL_ASYNC));
    receive_async.operationContext = &ctxReceive; 
    receive_async.completionFunction = (WSMAN_SHELL_COMPLETION_FUNCTION) &cbReceive;

    // Receive command output
    WSMAN_OPERATION_HANDLE receiveOperation = NULL;
	append(out, "[+] Receiving output\n");
    WSManReceiveShellOutput(shell, command, 0, NULL, &receive_async, &receiveOperation);
	WaitForSingleObject(ctxReceive.evt, INFINITE);
    if (NO_ERROR != ctxReceive.errCode) 
    {
        append(out, "WSManReceiveShellOutput failed: %d\n", ctxReceive.errCode);
        goto cleanup;
    }
	
	ret = 0;

    // Clean up
cleanup:
	append(out, "[+] Cleaning up now\n");
    if (command)
	{
		WSManCloseCommand(command, 0, &shell_async);
		WaitForSingleObject(ctx.evt, INFINITE);
		if (NO_ERROR != errCode) 
		{
			append(out, "WSManCloseCommand failed: %d\n", ctx.errCode);
			goto cleanup;
		}
	}
	
    if (shell) {
		WSManCloseShell(shell, 0, &shell_async);
		WaitForSingleObject(ctx.evt, INFINITE);
		if (NO_ERROR != errCode) 
		{
			append(out, "WSManCloseCommand failed: %d\n", ctx.errCode);
			goto cleanup;
		}
	}
	
    if (session) WSManCloseSession(session, 0);
    if (apiHandle) WSManDeinitialize(apiHandle, 0);
    if (ctx.evt) CloseHandle(ctx.evt);
    if (ctxReceive.evt) CloseHandle(ctxReceive.evt);
	

    return ret;
}

static char* find_arg(const char *prefix, char **args, const char *default_value) {
    while (*args) {
        if (strncmp(*args, prefix, strlen(prefix)) == 0) {
            return *args + strlen(prefix);
        }
        args++;
    }
    return (char *)default_value;
}