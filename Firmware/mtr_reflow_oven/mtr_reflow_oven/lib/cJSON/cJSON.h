/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  ... (MIT License) ...
*/
#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7)

#define cJSON_IsBool(cjson) (cjson && (cjson->type & (cJSON_False | cJSON_True)))
#define cJSON_IsNull(cjson) (cjson && (cjson->type & cJSON_NULL))
#define cJSON_IsNumber(cjson) (cjson && (cjson->type & cJSON_Number))
#define cJSON_IsString(cjson) (cjson && (cjson->type & cJSON_String))
#define cJSON_IsArray(cjson) (cjson && (cjson->type & cJSON_Array))
#define cJSON_IsObject(cjson) (cjson && (cjson->type & cJSON_Object))
#define cJSON_IsTrue(cjson) (cjson && (cjson->type & cJSON_True))
#define cJSON_IsFalse(cjson) (cjson && (cjson->type & cJSON_False))

typedef struct cJSON
{
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

/* Supply a block of JSON, and this returns a cJSON object you can interrogate. */
extern cJSON *cJSON_Parse(const char *value);
extern cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);
extern void cJSON_Delete(cJSON *c);

/* Get Keys */
extern int cJSON_GetArraySize(const cJSON *array);
extern cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
extern cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
extern int cJSON_HasObjectItem(const cJSON *object, const char *string); // Shim
extern const char *cJSON_GetErrorPtr(void);

/* Helper */
extern cJSON *cJSON_CreateObject(void);
extern void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
// ... partial implementation for reading mostly

#ifdef __cplusplus
}
#endif

#endif
