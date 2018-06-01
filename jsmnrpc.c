/**
@file    jsmnrpc_tiny.cpp
@author  lukasz.forynski
@date    06/08/2013
@brief   Very lightweight implementation of JSON-RPC framework for C or C++.
It allows registering handlers, supports request handling and does not perform
any memory allocations. It can be used in embedded systems.
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

#include <stdint.h>

#include "jsmnrpc.h"


/* Private types and definitions ------------------------------------------------------- */

static const char* response_1x_prefix = "{";
static const char* response_20_prefix = "{\"jsonrpc\": \"2.0\"";

typedef struct jsmnrpc_error_code
{
  const char* error_code;
  const char* error_msg;
} jsmnrpc_error_code_t;

jsmnrpc_error_code_t jsmnrpc_err_codes[] =
{
  { "-32700", "Parse error" },   /* An error occurred on the server while parsing the JSON text */
  { "-32600", "Invalid Request" },   /* The JSON sent is not a valid Request object */
  { "-32601", "Method not found" },   /* The method does not exist / is not available */
  { "-32602", "Invalid params" },   /* Invalid method parameter(s) */
  { "-32603", "Internal error" }    /* Internal JSON-RPC error */
};

enum jsmnrpc_key_ids
{
  jsmnrpc_key_jsonrpc = 0,
  jsmnrpc_key_method,
  jsmnrpc_key_params,
  jsmnrpc_key_id,
  jsmnrpc_key_result,
  jsmnrpc_key_error,
  jsmnrpc_key_count
};

const char* jsmnrpc_keys[] =
{
  "jsonrpc",
  "method",
  "params",
  "id",
  "result",
  "error",
};

/* Private function declarations ------------------------------------------------------- */
static int str_len(const char* str);

/* Exported functions ------------------------------------------------------- */
void jsmnrpc_init(jsmnrpc_instance_t* self, jsmnrpc_handler_t* table_for_handlers, int max_num_of_handlers)
{
  int i;
  self->handlers = table_for_handlers;
  self->num_of_handlers = 0;
  self->max_num_of_handlers = max_num_of_handlers;

  for (i = 0; i < self->max_num_of_handlers; i++)
  {
    self->handlers[i].handler_name = 0;
    self->handlers[i].handler = 0;
  }
}

void jsmnrpc_register_handler(jsmnrpc_instance_t* self, const char* handler_name, jsmnrpc_handler_callback_t handler)
{
  if (self->num_of_handlers < self->max_num_of_handlers)
  {
    if (handler_name && handler)
    {
      self->handlers[self->num_of_handlers].handler_name = handler_name;
      self->handlers[self->num_of_handlers].handler = handler;
      self->num_of_handlers++;
    }
  }
}

static int jsmnrpc_get_handler_id(jsmnrpc_instance_t* table, const jsmnrpc_string_t name)
{
  int result = -1;
  for (int i = 0; i < table->num_of_handlers; i++)
  {
    if (str_are_equal(name.data, name.length, table->handlers[i].handler_name)) {
      result = i;
      break;
    }
  }
  return result;
}

void jsmnrpc_handle_request_single(jsmnrpc_instance_t* self, jsmnrpc_request_info_t* request_info, int token_id)
{
  jsmnrpc_token_list_t *tokens = &request_info->data->tokens;
  int method_value_token = jsmnrpc_get_value(tokens, token_id, -1, jsmnrpc_keys[jsmnrpc_key_method]);
  int jsonrpc_value_token = jsmnrpc_get_value(tokens, token_id, -1, jsmnrpc_keys[jsmnrpc_key_jsonrpc]);
  request_info->params_value_token = jsmnrpc_get_value(tokens, token_id, -1, jsmnrpc_keys[jsmnrpc_key_params]);
  request_info->id_value_token = jsmnrpc_get_value(tokens, token_id, -1, jsmnrpc_keys[jsmnrpc_key_id]);
  request_info->info_flags = 0;

  if (jsonrpc_value_token > 0) {
    jsmnrpc_string_t str = jsmnrpc_get_string(tokens, jsonrpc_value_token);
    if (str_are_equal(str.data, str.length, "2.0")) {
      request_info->info_flags |= jsmnrpc_request_is_rpc_20;
    }
  }

  if (request_info->id_value_token < 0)
  {
    /* For rpc client 1.0, id must be null for notification, so when id is not exist, treat as error */
    if (!(request_info->info_flags & jsmnrpc_request_is_rpc_20)) {
      jsmnrpc_create_error(jsmnrpc_err_invalid_request, NULL, request_info);
      return;
    }
    request_info->info_flags |= jsmnrpc_request_is_notification;
    /*
    id
    An identifier established by the Client that MUST contain a String, Number, or NULL value if included.
    If it is not included it is assumed to be a notification. The value SHOULD normally not be Null [1]
    and Numbers SHOULD NOT contain fractional parts [2]
    The Server MUST reply with the same value in the Response object if included. This member is used to
    correlate the context between the two objects.

    [1] The use of Null as a value for the id member in a Request object is discouraged, because this
    specification uses a value of Null for Responses with an unknown id. Also, because JSON-RPC 1.0 uses
    an id value of Null for Notifications this could cause confusion in handling.

    [2] Fractional parts may be problematic, since many decimal fractions cannot be represented exactly
    as binary fractions.
    */
  }
  else
  {
    jsmntok_t *id_value_token = NULL;
    if (request_info->id_value_token >= 0) {
      id_value_token = tokens->data + request_info->id_value_token;
    }

    /* if id value is not primitive or string, then it's an error */
    if (id_value_token == NULL || (id_value_token->type != JSMN_PRIMITIVE && id_value_token->type != JSMN_STRING)) {
      jsmnrpc_create_error(jsmnrpc_err_invalid_request, NULL, request_info);
      return;
    }
    if (!(request_info->info_flags & jsmnrpc_request_is_rpc_20)) {
      /* For rpc client 1.0, null is notification */
      jsmnrpc_string_t str = jsmnrpc_get_string(tokens, request_info->id_value_token);
      if (str_are_equal(str.data, str.length, "null")) {
        request_info->info_flags |= jsmnrpc_request_is_notification;
      }
    }
  }

  {
    if (method_value_token >= 0 && tokens->data[method_value_token].type == JSMN_STRING) {
      jsmnrpc_string_t str = jsmnrpc_get_string(tokens, method_value_token);
      int handler_id = jsmnrpc_get_handler_id(self, str);
      if (handler_id >= 0) {
        self->handlers[handler_id].handler(request_info);
      }
      else
      {
        jsmnrpc_create_error(jsmnrpc_err_method_not_found, NULL, request_info);
      }
    }
    else
    {
      jsmnrpc_create_error(jsmnrpc_err_invalid_request, NULL, request_info);
    }
  }
}

bool jsmnrpc_parse(jsmnrpc_token_list_t* tokens, jsmnrpc_string_t *str)
{
  if (tokens) {
    tokens->length = -1;
    jsmn_init(&(tokens->parser));
    if (str) {
      tokens->json = str->data;
      tokens->length = jsmn_parse(&(tokens->parser), str->data, str->length, tokens->data, tokens->capacity);
      return tokens->length > 0;
    }
  }
  return false;
}

void jsmnrpc_handle_request(jsmnrpc_instance_t* self, jsmnrpc_data_t* request_data)
{
  jsmnrpc_request_info_t request_info;
  request_info.data = request_data;
  const int root_token_id = 0;
  jsmnrpc_token_list_t *tokens = &request_data->tokens;
  jsmnrpc_string_t *request = &request_data->request;
  jsmntok_t *root_token = tokens->data + root_token_id;
  request_data->response.length = 0;
  request_info.id_value_token = -1;
  request_info.params_value_token = -1;
  request_info.info_flags = 0;

  if (!jsmnrpc_parse(tokens, request)) {
    jsmnrpc_create_error(jsmnrpc_err_parse_error, NULL, &request_info);
    return;
  }

  if (root_token->type != JSMN_ARRAY && root_token->type != JSMN_OBJECT) {
    jsmnrpc_create_error(jsmnrpc_err_invalid_request, NULL, &request_info);
    return;
  }

  if (root_token->type == JSMN_ARRAY && root_token->size < 1) {
    jsmnrpc_create_error(jsmnrpc_err_invalid_request, NULL, &request_info);
    return;
  }

  if (root_token->type == JSMN_ARRAY)
  {
    append_str_with_len(&request_data->response, "[", SIZE_MAX);
    for (int i = 1; i < tokens->length; ++i) {
      if (tokens->data[i].parent == root_token_id) {
        jsmnrpc_handle_request_single(self, &request_info, i);
      }
    }
    append_str_with_len(&request_data->response, "]", SIZE_MAX);
  }
  else
  {
    jsmnrpc_handle_request_single(self, &request_info, 0);
  }
  if (request_data->response.capacity > 0)
  {
    if (request_data->response.length < request_data->response.capacity)
    {
      request_data->response.data[request_data->response.length] = 0;
    }
    else
    {
      request_data->response.data[request_data->response.capacity - 1] = 0;
    }
  }
}

bool jsmnrpc_create_result_prefix(jsmnrpc_request_info_t* info)
{
  if (!(info->info_flags & jsmnrpc_request_is_notification))
  {
    jsmnrpc_string_t *response = &info->data->response;
    if (response->length > 2) // not the beginning of a batch response
    {
      append_str_with_len(response, ", ", 2);
    }
    if (info->info_flags & jsmnrpc_request_is_rpc_20)
    {
      append_str_with_len(response, response_20_prefix, SIZE_MAX);
    }
    else
    {
      append_str_with_len(response, response_1x_prefix, SIZE_MAX);
      append_str_with_len(response, "\"error\": null", SIZE_MAX);
    }

    if (info->id_value_token >= 0)
    {
      append_str_with_len(response, ", \"id\": ", SIZE_MAX);
      append_str(response, jsmnrpc_get_string(&info->data->tokens, info->id_value_token));
    }
    return true;
  }
  return false;
}

void jsmnrpc_create_result(const char* result_str, jsmnrpc_request_info_t* info)
{
  if (jsmnrpc_create_result_prefix(info)) {
    jsmnrpc_string_t *response = &info->data->response;
    append_str_with_len(response, ", \"result\": ", SIZE_MAX);
    append_str_with_len(response, result_str, SIZE_MAX);
    append_str_with_len(response, "}", SIZE_MAX);
  }
}

void jsmnrpc_create_error(int err, const char* err_msg, jsmnrpc_request_info_t* info)
{
  char tmp_buffer[20];
  const char*err_code = NULL;
  jsmnrpc_string_t *response = &info->data->response;
  jsmnrpc_string_t *request = &info->data->request;
  if (err >= 0 && err < jsmnrpc_err_count) {
    err_code = jsmnrpc_err_codes[err].error_code;
    err_msg = jsmnrpc_err_codes[err].error_msg;
  }
  if (err_code == NULL) {
    err_code = i_to_str(err, tmp_buffer);
  }

  if (response->length > 2) // not the beginning of a batch response
  {
    append_str_with_len(response, ", ", 2);
  }

  if (!(info->info_flags & jsmnrpc_request_is_notification))
  {
    if (err == jsmnrpc_err_parse_error) {
      char *request_filterd = tmp_buffer;
      *request_filterd = 0;
      for (int i = 0; i < request->length && (request_filterd < tmp_buffer + 20); ++i) {
        char ch = request->data[i];
        if (ch != '\t' && ch != '\n' && ch != '\r' && ch != ' ') {
          *request_filterd = ch;
          ++request_filterd;
        }
      }
      
      if (str_are_equal(tmp_buffer, str_len("{\"jsonrpc\":\"2.0\","), "{\"jsonrpc\":\"2.0\",")) {
        info->info_flags |= jsmnrpc_request_is_rpc_20;
      }
    }

    if (info->info_flags & jsmnrpc_request_is_rpc_20)
    {
      append_str_with_len(response, response_20_prefix, SIZE_MAX);
      append_str_with_len(response, ", ", SIZE_MAX);
    }
    else
    {
      append_str_with_len(response, response_1x_prefix, SIZE_MAX);
    }

    append_str_with_len(response, "\"error\": {\"code\": ", SIZE_MAX);
    append_str_with_len(response, err_code, SIZE_MAX);
    append_str_with_len(response, ", \"message\": \"", SIZE_MAX);
    append_str_with_len(response, err_msg, SIZE_MAX);
    append_str_with_len(response, "\"}", SIZE_MAX);
    if (info->id_value_token >= 0 || err == jsmnrpc_err_invalid_request)
    {
      append_str_with_len(response, ", \"id\": ", SIZE_MAX);
      if (info->id_value_token >= 0)
      {
        append_str(response, jsmnrpc_get_string(&info->data->tokens, info->id_value_token));
      }
      else
      {
        append_str_with_len(response, "null", SIZE_MAX);
      }
    }
    append_str_with_len(response, "}", SIZE_MAX);
  }
}

int jsmnrpc_get_object_key(jsmnrpc_token_list_t *tokens, int token_offset, int index)
{
  int result = -1;
  int offset = 0;
  if (token_offset < 0 || tokens->data[token_offset].type != JSMN_OBJECT) {
    return -1;
  }
  for (int i = token_offset + 1; i < tokens->length; ++i) {
    jsmntok_t *token = tokens->data + i;
    if (token->parent == token_offset) {
      if (offset == index) {
        result = i;
        break;
      }
      ++offset;
    }
  }
  return result;
}

int jsmnrpc_get_value(jsmnrpc_token_list_t *tokens, int token_offset, int index, const char*key)
{
  int result = -1;
  if (token_offset >= 0)
  {
    jsmntok_t *node = tokens->data + token_offset;
    if (node->type == JSMN_ARRAY && index < 0)
    {
      result = -1;
    }
    else if (node->type == JSMN_OBJECT && key == NULL)
    {
      result = -1;
    }
    else if (node->type != JSMN_ARRAY && node->type != JSMN_OBJECT)
    {
      if ((token_offset + 1 < tokens->length) && tokens->data[token_offset + 1].parent == token_offset) {
        result = token_offset + 1;
      }
      else
      {
        result = token_offset;
      }
    }
    else
    {
      int offset = 0;
      for (int i = token_offset + 1; i < tokens->length; ++i) {
        jsmntok_t *token = tokens->data + i;
        if (token->parent == token_offset) {
          jsmnrpc_string_t str = jsmnrpc_get_string(tokens, i);
          if (node->type == JSMN_OBJECT) {
            if (str_are_equal(str.data, str.length, key)) {
              result = i + 1;
              /* Protecting result */
              if (result >= tokens->length || tokens->data[result].parent != i) {
                result = -1;
              }
              break;
            }
          }
          else if (node->type == JSMN_ARRAY)
          {
            if (offset == index)
            {
              result = i;
              break;
            }
            ++offset;
          }
        }
      }
    }
  }
  return result;
}

jsmnrpc_string_t jsmnrpc_get_string(jsmnrpc_token_list_t *tokens, int token) {
  jsmnrpc_string_t result;
  if (token < 0) {
    result.data = NULL;
    result.length = 0;
  } else {
    result.data = (char*)(tokens->json + tokens->data[token].start);
    result.length = tokens->data[token].end - tokens->data[token].start;
  }
  result.capacity = 0;
  return result;
}

void append_str_with_len(jsmnrpc_string_t *str, const char* from, size_t len)
{
  size_t saved_len = str->length;
  if (len == SIZE_MAX) {
    len = str_len(from);
  }
  str->length += len;
  if (str->length < str->capacity) {
    for (int i = 0; i < len; ++i) {
      str->data[saved_len + i] = from[i];
    }
  }
}

void append_str(jsmnrpc_string_t *str, jsmnrpc_string_t rpc_str)
{
  append_str_with_len(str, rpc_str.data, rpc_str.length);
}

int str_are_equal(const char* first, size_t first_len, const char* second_zero_ended)
{
  int are_equal = 0;
  while (*second_zero_ended != 0 &&  // not the end of string?
    *first != 0 &&    // nor end of str
    *first == *second_zero_ended) // until are equal
  {
    first++;
    second_zero_ended++;
    first_len--;
  }

  // here, if we're at the end of first and second they do match at least at first_len characters.
  if (*second_zero_ended == 0 && first_len == 0)
  {
    are_equal = 1;
  }
  return are_equal;
}

char* i_to_str(int i, char b[])
{
  char const digit[] = "0123456789";
  char* p = b;
  if (i<0) {
    *p++ = '-';
    i *= -1;
  }
  int shifter = i;
  do { //Move to where representation ends
    ++p;
    shifter = shifter / 10;
  } while (shifter);
  *p = '\0';
  do { //Move back, inserting digits as u go
    *--p = digit[i % 10];
    i = i / 10;
  } while (i);
  return b;
}

static int int_val(char symbol, int* result)
{
  int value_ok = 1;
  int correct_by = 0;

  if ((symbol >= '0') && (symbol <= '9'))
  {
    correct_by = '0';
  }
  else if ((symbol >= 'A') && (symbol <= 'F'))
  {
    correct_by = ('A' - 10);
  }
  else if ((symbol >= 'a') && (symbol <= 'f'))
  {
    correct_by = ('a' - 10);
  }
  else
  {
    value_ok = 0;
  }

  *result = symbol - correct_by;
  return value_ok;
}

int str_to_i(const char* start, size_t length, int* result)
{
  int extracted_ok = 1;
  int base = 10;
  int value = 0;
  int sign = 1;
  int val_start = 0;
  int i = 0;

  if (length > 0 && start[0] == '-')
  {
    sign = -1;
    val_start++;
  }

  // find base: 0x: hex(16), 0:octal(8)
  if (length > val_start + 2 &&
    start[val_start] == '0')
  {
    if (start[val_start + 1] == 'x')
    {
      base = 16;
      val_start += 2;
    }
    else
    {
      base = 8;
      val_start += 1;
    }
  }

  *result = 0;
  for (i = val_start; i < length; i++)
  {
    if (i > val_start)
    {
      *result *= base;
    }

    if (!int_val(start[i], &value) ||
      value > base)
    {
      extracted_ok = 0;
      break;
    }
    *result += value;
  }
  *result *= sign;
  return extracted_ok;
}

int str_len(const char* str)
{
  // yes yes, the unsafe str_len.. hopefully we'll use it wisely ;)
  int i = 0;
  while (*str++)
  {
    i++;
  }
  return i;
}
