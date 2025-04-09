#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>

DosatoFunctionMapList functions;
void* main_vm;

int main1(void);

void init(void* vm) {
    // must set vm to the main_vm, in order for the garbage collector to work, this is required
    main_vm = vm;

    // make sure to initialize the functions list
    init_DosatoFunctionMapList(&functions);

    // add the function to the list, this is required for the garbage collector to work, this is required
    write_DosatoFunctionMapList(&functions, (DosatoFunctionMap) { "startServer", startServer });
    write_DosatoFunctionMapList(&functions, (DosatoFunctionMap) { "sendMessage", sendMessage });
}

bool started = false;

Value onopenFunction;
Value oncloseFunction;
Value onmessageFunction;

void onopen(ws_cli_conn_t client);
void onclose(ws_cli_conn_t client);
void onmessage(ws_cli_conn_t client, const unsigned char *msg, uint64_t size, int type);\
Value sendMessage(ValueArray args, bool debug) {
    if (args.count != 3) {
        return BUILD_EXCEPTION(E_WRONG_NUMBER_OF_ARGUMENTS);
    }

    Value client = GET_ARG(args, 0);
    Value message = GET_ARG(args, 1);
    Value type = GET_ARG(args, 2);

    CAST_SAFE(client, TYPE_ULONG);
    CAST_SAFE(message, TYPE_ARRAY);
    CAST_SAFE(type, TYPE_INT);

    ws_cli_conn_t client_num = AS_ULONG(client);
    ValueArray* byte_arr = AS_ARRAY(message);
    unsigned char* msg = malloc(byte_arr->count);
    for (int i = 0; i < byte_arr->count; i++) {
        msg[i] = AS_UBYTE(byte_arr->values[i]);
    }
    int type_num = AS_INT(type);
    // send the message to the client
    int result = ws_sendframe(client_num, (const char*)msg, byte_arr->count, type_num);
    free(msg);

    if (result == -1) {
        PRINT_ERROR("Error sending message to client %llu\n", (long long unsigned int)client_num);
        return BUILD_EXCEPTION(E_EMPTY_EXPRESSION);
    }
    return BUILD_NULL();
}

Value startServer(ValueArray args, bool debug) {
    if (args.count != 6) {
        return BUILD_EXCEPTION(E_WRONG_NUMBER_OF_ARGUMENTS);
    }

    Value host = GET_ARG(args, 0);
    CAST_SAFE(host, TYPE_STRING);
    Value port = GET_ARG(args, 1);
    CAST_SAFE(port, TYPE_USHORT);
    Value timeout = GET_ARG(args, 2);
    CAST_SAFE(timeout, TYPE_UINT);

    Value onopen_arg = GET_ARG(args, 3);
    CAST_SAFE(onopen_arg, TYPE_FUNCTION);
    Value onclose_arg = GET_ARG(args, 4);
    CAST_SAFE(onclose_arg, TYPE_FUNCTION);
    Value onmessage_arg = GET_ARG(args, 5);
    CAST_SAFE(onmessage_arg, TYPE_FUNCTION);

    char* host_str = AS_STRING(host);
    uint16_t port_num = AS_USHORT(port);
    uint32_t timeout_num = AS_UINT(timeout);

    onopenFunction = onopen_arg;
    oncloseFunction = onclose_arg;
    onmessageFunction = onmessage_arg;

    started = true;
    // start the server
    ws_socket(&(struct ws_server){
        .host = host_str,
        .port = port_num,
        .thread_loop   = 0,
        .timeout_ms    = timeout_num,
        .evs.onopen    = &onopen,
        .evs.onclose   = &onclose,
        .evs.onmessage = &onmessage
    });
    started = false;

    return BUILD_NULL();
}


void onopen(ws_cli_conn_t client) {
    char *cli;
    cli = ws_getaddress(client);

    // call the onopen function
    ValueArray args;
    init_ValueArray(&args);
    write_ValueArray(&args, BUILD_ULONG((long long int)client));
    write_ValueArray(&args, BUILD_STRING(cli));
    
    callExternalFunction(onopenFunction, args, false);
    free_ValueArray(&args);
}

void onclose(ws_cli_conn_t client) {
    char *cli;
    cli = ws_getaddress(client);

    // call the onclose function
    ValueArray args;
    init_ValueArray(&args);
    write_ValueArray(&args, BUILD_ULONG((long long int)client));
    write_ValueArray(&args, BUILD_STRING(cli));

    callExternalFunction(oncloseFunction, args, false);
    free_ValueArray(&args);
}

void onmessage(ws_cli_conn_t client, const unsigned char *msg, uint64_t size, int type) {
    char *cli;
    cli = ws_getaddress(client);

    // byte array
    ValueArray* byte_arr = malloc(sizeof(ValueArray));
    init_ValueArray(byte_arr);
    for (int i = 0; i < size; i++) {
        write_ValueArray(byte_arr, BUILD_UBYTE(msg[i]));
    }

    ValueArray args;
    init_ValueArray(&args);
    write_ValueArray(&args, BUILD_ULONG((long long int)client));
    write_ValueArray(&args, BUILD_STRING(cli));
    write_ValueArray(&args, BUILD_ARRAY(byte_arr));
    write_ValueArray(&args, BUILD_ULONG(size));
    write_ValueArray(&args, BUILD_INT(type));

    callExternalFunction(onmessageFunction, args, false);
    free_ValueArray(&args);
}