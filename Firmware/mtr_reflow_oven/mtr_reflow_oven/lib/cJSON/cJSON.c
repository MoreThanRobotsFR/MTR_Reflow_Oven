/*
  Simplified cJSON Implementation for Embedded
*/
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"

static const char *ep;

const char *cJSON_GetErrorPtr(void) { return ep; }

static int cJSON_strcasecmp(const char *s1, const char *s2)
{
    if (!s1) return (s1 == s2) ? 0 : 1;
    if (!s2) return 1;
    for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)
        if(*s1 == 0) return 0;
    return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static char* cJSON_strdup(const char* str)
{
      size_t len;
      char* copy;

      len = strlen(str) + 1;
      if (!(copy = (char*)cJSON_malloc(len))) return 0;
      memcpy(copy, str, len);
      return copy;
}

void cJSON_Delete(cJSON *c)
{
    cJSON *next;
    while (c)
    {
        next = c->next;
        if (!(c->type & cJSON_IsReference) && c->child) cJSON_Delete(c->child);
        if (!(c->type & cJSON_IsReference) && c->valuestring) cJSON_free(c->valuestring);
        if (!(c->type & cJSON_StringIsConst) && c->string) cJSON_free(c->string);
        cJSON_free(c);
        c = next;
    }
}

static const char *parse_number(cJSON *item, const char *num)
{
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;

    if (*num == '-') sign = -1, num++;
    if (*num == '0') num++;
    if (*num >= '1' && *num <= '9') do n = (n * 10.0) + (*num++ - '0'); while (*num >= '0' && *num <= '9');
    if (*num == '.' && num[1] >= '0' && num[1] <= '9') { num++; do n = (n * 10.0) + (*num++ - '0'), scale--; while (*num >= '0' && *num <= '9'); }
    if (*num == 'e' || *num == 'E')
    {
        num++; if (*num == '+') num++; else if (*num == '-') signsubscale = -1, num++;
        while (*num >= '0' && *num <= '9') subscale = (subscale * 10) + (*num++ - '0');
    }

    n = sign * n * pow(10.0, (scale + subscale * signsubscale));
    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return num;
}

static unsigned parse_hex4(const char *str)
{
    unsigned h = 0;
    if (*str >= '0' && *str <= '9') h += (*str) - '0'; else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A'; else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a'; else return 0;
    h = h << 4; str++;
    if (*str >= '0' && *str <= '9') h += (*str) - '0'; else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A'; else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a'; else return 0;
    h = h << 4; str++;
    if (*str >= '0' && *str <= '9') h += (*str) - '0'; else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A'; else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a'; else return 0;
    h = h << 4; str++;
    if (*str >= '0' && *str <= '9') h += (*str) - '0'; else if (*str >= 'A' && *str <= 'F') h += 10 + (*str) - 'A'; else if (*str >= 'a' && *str <= 'f') h += 10 + (*str) - 'a'; else return 0;
    return h;
}

static const char *parse_string(cJSON *item, const char *str)
{
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;
    unsigned uc, uc2;
    if (*str != '\"') { ep = str; return 0; }

    while (*ptr != '\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;

    out = (char*)cJSON_malloc(len + 1);
    if (!out) return 0;

    ptr = str + 1; ptr2 = out;
    while (*ptr != '\"' && *ptr)
    {
        if (*ptr != '\\') *ptr2++ = *ptr++;
        else
        {
            ptr++;
            switch (*ptr)
            {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                case 'u':
                    uc = parse_hex4(ptr + 1); ptr += 4;
                    if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break;
                    if (uc >= 0xD800 && uc <= 0xDBFF)
                    {
                        if (ptr[1] != '\\' || ptr[2] != 'u') break;
                        uc2 = parse_hex4(ptr + 3); ptr += 6;
                        if (uc2 < 0xDC00 || uc2 > 0xDFFF) break;
                        uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                    }
                    len = 4; if (uc < 0x80) len = 1; else if (uc < 0x800) len = 2; else if (uc < 0x10000) len = 3;
                    ptr2 += len;
                    switch (len) {
                        case 4: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                        case 3: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                        case 2: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                        case 1: *--ptr2 = (uc | ((0xFF >> len) << len));
                    }
                    ptr2 += len;
                    break;
                default: *ptr2++ = *ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = 0;
    if (*ptr == '\"') ptr++;
    item->valuestring = out;
    item->type = cJSON_String;
    return ptr;
}

static const char *skip(const char *in) { while (in && *in && (unsigned char)*in <= 32) in++; return in; }

static cJSON *cJSON_New_Item(void)
{
    cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

static const char *parse_value(cJSON *item, const char *value);

static const char *parse_array(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '[') { ep = value; return 0; }

    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return 0;
    value = skip(parse_value(child, skip(value)));
    if (!value) return 0;

    while (*value == ',')
    {
        cJSON *new_item;
        if (!(new_item = cJSON_New_Item())) return 0;
        child->next = new_item; new_item->prev = child; child = new_item;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return 0;
    }

    if (*value == ']') return value + 1;
    ep = value; return 0;
}

static const char *parse_object(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '{') { ep = value; return 0; }

    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return 0;
    value = skip(parse_string(child, skip(value)));
    if (!value) return 0;
    child->string = child->valuestring; child->valuestring = 0;
    if (*value != ':') { ep = value; return 0; }
    value = skip(parse_value(child, skip(value + 1)));
    if (!value) return 0;

    while (*value == ',')
    {
        cJSON *new_item;
        if (!(new_item = cJSON_New_Item())) return 0;
        child->next = new_item; new_item->prev = child; child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value) return 0;
        child->string = child->valuestring; child->valuestring = 0;
        if (*value != ':') { ep = value; return 0; }
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return 0;
    }

    if (*value == '}') return value + 1;
    ep = value; return 0;
}

static const char *parse_value(cJSON *item, const char *value)
{
    if (!value) return 0;
    if (!strncmp(value, "null", 4)) { item->type = cJSON_NULL;  return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4)) { item->type = cJSON_True; item->valueint = 1; return value + 4; }
    if (*value == '\"') { return parse_string(item, value); }
    if (*value == '-' || (*value >= '0' && *value <= '9')) { return parse_number(item, value); }
    if (*value == '[') { return parse_array(item, value); }
    if (*value == '{') { return parse_object(item, value); }

    ep = value; return 0;
}

cJSON *cJSON_Parse(const char *value) { return cJSON_ParseWithOpts(value, 0, 0); }
cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated)
{
    const char *end = 0;
    cJSON *c = cJSON_New_Item();
    ep = 0;
    if (!c) return 0;

    end = parse_value(c, skip(value));
    if (!end) { cJSON_Delete(c); return 0; }
    if (require_null_terminated) { end = skip(end); if (*end) { cJSON_Delete(c); ep = end; return 0; } }
    if (return_parse_end) *return_parse_end = end;
    return c;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string)
{
    cJSON *c = object->child;
    while (c && cJSON_strcasecmp(c->string, string)) c = c->next;
    return c;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index)
{
    cJSON *c = array->child;
    while (c && index > 0) { index--; c = c->next; }
    return c;
}

int cJSON_GetArraySize(const cJSON *array)
{
    cJSON *c = array->child;
    int i = 0;
    while (c) { i++; c = c->next; }
    return i;
}
