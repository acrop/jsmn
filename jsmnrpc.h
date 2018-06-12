/**
@file    jsmnrpc.h
@author  lukasz.forynski
@date    06/08/2013
@brief   Very lightweight implementation of JSON-RPC framework for C.
___________________________

The MIT License (MIT)

Copyright (c) 2013 Lukasz Forynski

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#pragma once
#ifndef _jsmnrpc_h_
#define _jsmnrpc_h_

#include <stdint.h>
#include <stdbool.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jsmnrpc_string {
  char* data;
  size_t length;
  size_t capacity;
} jsmnrpc_string_t;

typedef struct jsmnrpc_token_list {
  char* json;
  jsmntok_t* data;
  jsmn_size_t length;
  jsmn_size_t capacity;
  jsmn_parser parser;
} jsmnrpc_token_list_t;

/**
* @brief Structure defining rpc data info. Object of such a structure
*        is to be used to pass information about request/response buffers.
*/
typedef struct jsmnrpc_data
{
  jsmnrpc_string_t request;
  jsmnrpc_string_t response;
  jsmnrpc_token_list_t tokens;
  void*       arg;
  uint16_t info_flags;
} jsmnrpc_data_t;

/**
* @brief Structure containing all information about the request.
*        Pointer to such a structure will be passed to each handler, so that
*        handlers could access all required request information (and pass
*        this information to other extraction-aiding functions).
*/
typedef struct jsmnrpc_request_info
{
  jsmnrpc_data_t *data;
  jsmn_size_t params_value_token;
  jsmn_size_t id_value_token;
  uint16_t info_flags;
} jsmnrpc_request_info_t;

/**
* @brief  Definition of a handler type for RPC handlers.
* @param rpc_request pointer to original JSON-RPC request string that was received.
* @param info rpc_request_info with parsing results. This structure will be
*        initialised to point at 'params', so:
*        - params_start/params_len within rpc_request points at string containing parameters
*          for this RPC call. Handler implementation should use params_start, params_len to extract
*          those parameters as appropriate.
* @return pointer to the JSON-RPC complaint response (if 'id' is > 0): request, or NULL (if id < 0): notification
*/
typedef void (*jsmnrpc_handler_callback_t)(jsmnrpc_request_info_t* info);

/**
* @brief Structure used to define a storage for the service/function handler.
*        It should be used to define storage for the JSON-RPC instance
*        (table for handlers).
*/
typedef struct jsmnrpc_handler
{
  jsmnrpc_handler_callback_t handler;
  const char* handler_name;
} jsmnrpc_handler_t;

/**
* @brief Struct defining and instance of the JSON-RPC handling entity.
*        Number of different entities can be used (also from different threads),
*        and this structure is used to define memory used by such an instance.
*/
typedef struct jsmnrpc_instance
{
  jsmnrpc_handler_t* handlers;
  int num_of_handlers;
  int max_num_of_handlers;
} jsmnrpc_instance_t;

/**
* @brief Enumeration to allow selecting JSON-RPC2.0 error codes.
*/
enum jsmnrpc_20_errors
{
  jsmnrpc_err_parse_error = 0,       /* An error occurred on the server while parsing the JSON text */
  jsmnrpc_err_invalid_request,       /* The JSON sent is not a valid Request object */
  jsmnrpc_err_method_not_found,      /* The method does not exist / is not available */
  jsmnrpc_err_invalid_params,        /* Invalid method parameter(s) */
  jsmnrpc_err_internal_error,        /* Internal JSON-RPC error */
  jsmnrpc_err_count,                   /* JSON RPC 20 error count*/
};

enum jsmrpc_flags
{
  jsmnrpc_request_is_notification = 1,
  jsmnrpc_request_is_rpc_20 = 2,
  jsmnrpc_response_is_error = 4,
  jsmnrpc_response_is_result = 8,
  jsmnrpc_response_is_array = 16,
};

/* Exported functions ------------------------------------------------------- */

/* RPC specific functions ---------------------------------------------- */

/**
* @brief initialise rpc instance with a storage for handlers.
* @param self pointer to the jsmnrpc_instance_t object.
* @param table_for_handlers pointer to an allocated table that will hold handlers for this instance
* @param max_num_of_handlers number of items above table can hold.
*/
void jsmnrpc_init(jsmnrpc_instance_t* self, jsmnrpc_handler_t* table_for_handlers, int max_num_of_handlers);

bool jsmnrpc_parse(jsmnrpc_token_list_t* tokens, jsmnrpc_string_t *str);

/**
* @brief Registers a new handler.
* @param self pointer to the jsmnrpc_instance_t object.
* @param handler_name name of the function (as it appears in RCP request).
* @param handler pointer to the function handler (function of jsmnrpc_handler_fcn type).
*/
void jsmnrpc_register_handler(jsmnrpc_instance_t* self, const char* handler_name, jsmnrpc_handler_callback_t handler);


/**
* @brief Method to handle RPC request. As a result, one of the registered handlers might be executed
*        (if the function name from RFC request matches name for which a handler was registered).
* @param self pointer to the jsmnrpc_instance_t object.
* @param request_data pointer to a structure holding information about the request string,
*        information where the resulting response is to be stored (if any), and additional information
*        to be passed to the handler (see jsmnrpc_data_t for more info).
* @return Pointer to buffer containing the response (the same buffer as passed in request_data).
*         If the request was a notification only, this buffer will be empty.
*/
void jsmnrpc_handle_request(jsmnrpc_instance_t* self, jsmnrpc_data_t* request_data);

bool jsmnrpc_create_result_prefix(jsmnrpc_request_info_t* info);


/**
* @brief Function to create an RPC response. It is designed to be used
*        in the handler to create RCP response (in the response buffer).
*        This function will either fill out the buffer with a valid response, or null-terminate
*        it at it's beginning for notification, so there is no need to check that in the handler.
*        This allows handlers to be implemented the same way for requests and notifications.
* @param result_str string (null terminated!) to be copied to the response.
* @param info pointer to the jsmnrpc_request_info_t structure that was passed to the handler.
*/
void jsmnrpc_create_result(const char* result_str, jsmnrpc_request_info_t* info);


/**
* @brief Function to create an RPC error response. It is designed to be used
*        in the handler to create RCP response (in the response buffer).
*        This function will either fill out the buffer with a valid error response, or null-terminate
*        it at it's beginning for notification, so there is no need to check that in the handler.
*        This allows handlers to be implemented the same way for requests and notifications.
* @param error_code - one of jsmnrpc_20_errors. Error will be constructed as per jsmnrpc_ 2.0 definitions.
* @param error_msg - The user defined error message.
* @param info pointer to the jsmnrpc_request_info_t structure that was passed to the handler.
*/
void jsmnrpc_create_error(int err_code, const char* err_msg, jsmnrpc_request_info_t* info);

/*
* @breif Function to get JSON object's key of index-th children,
*        through the key, we could be able retrieve the value correspond to the key
* @param tokens - The jsmn parsed token list.
* @param token_offset - The JSON node's offset in token list, the type of token must be JSMN_OBJECT
* @param index - Which object key want to retrieve
* @return The token offset id of the key
*/
int jsmnrpc_get_object_key(jsmnrpc_token_list_t *tokens, int token_offset, int index);

/*
* @breif Function to get JSON object's member value for key, or an object key's correspond value,
*        or array's index-th value
* @param tokens - The jsmn parsed token list.
* @param token_offset - The JSON node's offset in tokens, the type of token maybe any one of jsmntype_t
* @param index - The index of JSON array when token_offset's type is Array when type of token_offset are JSMN_ARRAY,
         otherwise it's should be -1 
* @param key - The key of JSON object when token_offset's type is JSMN_OBJECT, otherwise it's should be NULL
* @return The value node's offset in token list(tokens).
*/
int jsmnrpc_get_value(jsmnrpc_token_list_t *tokens, int token_offset,  int index, const char*key);

inline jsmntype_t jsmnrpc_get_token_type(jsmnrpc_token_list_t *tokens, int token_id) {
  if (token_id < 0) {
    return JSMN_UNDEFINED;
  }
  return tokens->data[token_id].type;
}
jsmnrpc_string_t jsmnrpc_get_string(jsmnrpc_token_list_t *tokens, int token);

void append_str_with_len(jsmnrpc_string_t *str, const char* from, size_t len);
void append_str(jsmnrpc_string_t *str, jsmnrpc_string_t rpc_str);

int str_are_equal(const char* first, size_t first_len, const char* second_zero_ended);
char* i_to_str(int i, char b[]);
int str_to_i(const char* start, size_t length, int* result);
int str_len(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* _jsmnrpc_h_ */
