/**
@file    json_rpc_tiny.h
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

#ifndef JSON_RPC_TINY
#define JSON_RPC_TINY

#include <stdint.h>
#include <stdbool.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_rpc_string {
  char* str;
  int length;
  int capacity;
} json_rpc_string_t;

/**
* @brief Structure defining rpc data info. Object of such a structure
*        is to be used to pass information about request/response buffers.
*/
typedef struct json_rpc_data
{
  json_rpc_string_t request;
  json_rpc_string_t response;
  jsmntok_t*  tokens;
  int         tokens_len;
  int         tokens_capacity;
  jsmn_parser parser;
  void*       arg;
} json_rpc_data_t;

/**
* @brief Structure containing all information about the request.
*        Pointer to such a structure will be passed to each handler, so that
*        handlers could access all required request information (and pass
*        this information to other extraction-aiding functions).
*/
typedef struct rpc_request_info
{
  json_rpc_data_t *data;
  uint32_t params_token;
  uint32_t id_token;
  uint16_t info_flags;
} rpc_request_info_t;

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
typedef void (*json_rpc_handler_callback_t)(rpc_request_info_t* info);

/**
* @brief Structure used to define a storage for the service/function handler.
*        It should be used to define storage for the JSON-RPC instance
*        (table for handlers).
*/
typedef struct json_rpc_handler
{
  json_rpc_handler_callback_t handler;
  const char* handler_name;
} json_rpc_handler_t;

/**
* @brief Struct defining and instance of the JSON-RPC handling entity.
*        Number of different entities can be used (also from different threads),
*        and this structure is used to define memory used by such an instance.
*/
typedef struct json_rpc_instance
{
  json_rpc_handler_t* handlers;
  int num_of_handlers;
  int max_num_of_handlers;
} json_rpc_instance_t;

/**
* @brief Enumeration to allow selecting JSON-RPC2.0 error codes.
*/
enum json_rpc_20_errors
{
  json_rpc_err_parse_error = 0,       /* An error occurred on the server while parsing the JSON text */
  json_rpc_err_invalid_request,       /* The JSON sent is not a valid Request object */
  json_rpc_err_method_not_found,      /* The method does not exist / is not available */
  json_rpc_err_invalid_params,        /* Invalid method parameter(s) */
  json_rpc_err_internal_error,        /* Internal JSON-RPC error */
  json_rpc_err_count,                   /* JSON RPC 20 error count*/
};

enum request_info_flags
{
  rpc_request_is_notification = 1,
  rpc_request_is_rpc_20 = 2
};

/* Exported functions ------------------------------------------------------- */

/* RPC specific functions ---------------------------------------------- */

/**
* @brief initialise rpc instance with a storage for handlers.
* @param self pointer to the json_rpc_instance_t object.
* @param table_for_handlers pointer to an allocated table that will hold handlers for this instance
* @param max_num_of_handlers number of items above table can hold.
*/
void json_rpc_init(json_rpc_instance_t* self, json_rpc_handler_t* table_for_handlers, int max_num_of_handlers);


/**
* @brief Registers a new handler.
* @param self pointer to the json_rpc_instance_t object.
* @param fcn_name name of the function (as it appears in RCP request).
* @param handler pointer to the function handler (function of json_rpc_handler_fcn type).
*/
void json_rpc_register_handler(json_rpc_instance_t* self, const char* fcn_name, json_rpc_handler_callback_t handler);


/**
* @brief Method to handle RPC request. As a result, one of the registered handlers might be executed
*        (if the function name from RFC request matches name for which a handler was registered).
* @param self pointer to the json_rpc_instance_t object.
* @param request_data pointer to a structure holding information about the request string,
*        information where the resulting response is to be stored (if any), and additional information
*        to be passed to the handler (see json_rpc_data_t for more info).
* @return Pointer to buffer containing the response (the same buffer as passed in request_data).
*         If the request was a notification only, this buffer will be empty.
*/
void json_rpc_handle_request(json_rpc_instance_t* self, json_rpc_data_t* request_data);

bool json_rpc_create_result_prefix(rpc_request_info_t* info);


/**
* @brief Function to create an RPC response. It is designed to be used
*        in the handler to create RCP response (in the response buffer).
*        This function will either fill out the buffer with a valid response, or null-terminate
*        it at it's beginning for notification, so there is no need to check that in the handler.
*        This allows handlers to be implemented the same way for requests and notifications.
* @param result_str string (null terminated!) to be copied to the response.
* @param info pointer to the rpc_request_info_t structure that was passed to the handler.
*/
void json_rpc_create_result(const char* result_str, rpc_request_info_t* info);


/**
* @brief Function to create an RPC error response. It is designed to be used
*        in the handler to create RCP response (in the response buffer).
*        This function will either fill out the buffer with a valid error response, or null-terminate
*        it at it's beginning for notification, so there is no need to check that in the handler.
*        This allows handlers to be implemented the same way for requests and notifications.
* @param error_code - one of json_20_errors. Error will be constructed as per JSON_RPC 2.0 definitions.
* @param info pointer to the rpc_request_info_t structure that was passed to the handler.
*/
void json_rpc_create_error(int err_code, const char* err_msg, rpc_request_info_t* info);

int json_rpc_get_array_member(json_rpc_data_t *data, int index, int token_id);
int json_rpc_get_object_member(json_rpc_data_t *data, const char*key, int token_id);
inline jsmntype_t json_rpc_get_token_type(json_rpc_data_t *data, int token_id) {
  if (token_id < 0) {
    return JSMN_UNDEFINED;
  }
  return data->tokens[token_id].type;
}
json_rpc_string_t json_rpc_get_string(json_rpc_data_t *data, int token);

void append_str_with_len(json_rpc_string_t *str, const char* from, int len);
void append_str(json_rpc_string_t *str, json_rpc_string_t rpc_str);

int str_are_equal(const char* first, int first_len, const char* second_zero_ended);


#ifdef __cplusplus
}
#endif

#endif /* JSON_RPC_TINY */
