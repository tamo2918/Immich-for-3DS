/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free_fn)(void *ptr) = free;

static const char *global_ep = NULL;

const char *cJSON_GetErrorPtr(void) { return global_ep; }

static char* cJSON_strdup(const char* str)
{
    size_t len;
    char* copy;

    if (!str) return NULL;
    len = strlen(str) + 1;
    copy = (char*)cJSON_malloc(len);
    if (!copy) return NULL;
    memcpy(copy, str, len);
    return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (!hooks) {
        cJSON_malloc = malloc;
        cJSON_free_fn = free;
        return;
    }
    cJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
    cJSON_free_fn = (hooks->free_fn)?hooks->free_fn:free;
}

static cJSON *cJSON_New_Item(void)
{
    cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node) memset(node, '\0', sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *c)
{
    cJSON *next;
    while (c)
    {
        next = c->next;
        if (!(c->type & cJSON_IsReference) && c->child) cJSON_Delete(c->child);
        if (!(c->type & cJSON_IsReference) && c->valuestring) cJSON_free_fn(c->valuestring);
        if (!(c->type & cJSON_StringIsConst) && c->string) cJSON_free_fn(c->string);
        cJSON_free_fn(c);
        c = next;
    }
}

void cJSON_free(void *object) {
    cJSON_free_fn(object);
}

static const char *parse_value(cJSON *item, const char *value);
static char *print_value(const cJSON *item, int depth, int fmt);
static const char *parse_array(cJSON *item, const char *value);
static char *print_array(const cJSON *item, int depth, int fmt);
static const char *parse_object(cJSON *item, const char *value);
static char *print_object(const cJSON *item, int depth, int fmt);

static const char *skip(const char *in) { while (in && *in && (unsigned char)*in <= 32) in++; return in; }

cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length) {
    (void)buffer_length;
    return cJSON_Parse(value);
}

cJSON *cJSON_Parse(const char *value)
{
    const char *end = 0;
    cJSON *c = cJSON_New_Item();
    global_ep = 0;
    if (!c) return 0;

    end = parse_value(c, skip(value));
    if (!end) { cJSON_Delete(c); return 0; }
    return c;
}

char *cJSON_Print(const cJSON *item) { return print_value(item, 0, 1); }
char *cJSON_PrintUnformatted(const cJSON *item) { return print_value(item, 0, 0); }
char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt) {
    (void)prebuffer;
    return print_value(item, 0, fmt);
}

static const char *parse_number(cJSON *item, const char *num)
{
    double n = 0, sign = 1, scale = 0; int subscale = 0, signsubscale = 1;

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

static char *print_number(const cJSON *item)
{
    char *str = (char*)cJSON_malloc(64);
    if (!str) return NULL;
    double d = item->valuedouble;
    if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN)
        snprintf(str, 64, "%d", item->valueint);
    else {
        if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60)
            snprintf(str, 64, "%.0f", d);
        else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
            snprintf(str, 64, "%e", d);
        else
            snprintf(str, 64, "%f", d);
    }
    return str;
}

static const char *parse_hex4(const char *str, unsigned int *out)
{
    unsigned int h = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)*str++;
        if (c >= '0' && c <= '9') h = (h << 4) + (c - '0');
        else if (c >= 'a' && c <= 'f') h = (h << 4) + 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') h = (h << 4) + 10 + (c - 'A');
        else return 0;
    }
    *out = h;
    return str;
}

static const char *parse_string(cJSON *item, const char *str)
{
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;

    if (*str != '\"') { global_ep = str; return 0; }

    while (*ptr != '\"' && *ptr && ++len) { if (*ptr++ == '\\') ptr++; }

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
                case '\"': case '\\': case '/': *ptr2++ = *ptr; break;
                case 'u': {
                    unsigned int uc;
                    ptr = parse_hex4(ptr + 1, &uc);
                    if (!ptr) { cJSON_free_fn(out); return 0; }
                    if (uc < 0x80) *ptr2++ = (char)uc;
                    else if (uc < 0x800) { *ptr2++ = (char)(0xC0 | (uc >> 6)); *ptr2++ = (char)(0x80 | (uc & 0x3F)); }
                    else { *ptr2++ = (char)(0xE0 | (uc >> 12)); *ptr2++ = (char)(0x80 | ((uc >> 6) & 0x3F)); *ptr2++ = (char)(0x80 | (uc & 0x3F)); }
                    ptr--;
                    break;
                }
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

static char *print_string_ptr(const char *str)
{
    const char *ptr; char *ptr2, *out; int len = 0;
    if (!str) return cJSON_strdup("\"\"");
    for (ptr = str; *ptr; ptr++) len += (*ptr == '\"' || *ptr == '\\' || (unsigned char)*ptr < 32) ? 2 : 1;
    out = (char*)cJSON_malloc(len + 3);
    if (!out) return 0;
    ptr2 = out; *ptr2++ = '\"';
    for (ptr = str; *ptr; ptr++) {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\') *ptr2++ = *ptr;
        else {
            *ptr2++ = '\\';
            switch (*ptr) {
                case '\\': *ptr2++ = '\\'; break;
                case '\"': *ptr2++ = '\"'; break;
                case '\b': *ptr2++ = 'b'; break;
                case '\f': *ptr2++ = 'f'; break;
                case '\n': *ptr2++ = 'n'; break;
                case '\r': *ptr2++ = 'r'; break;
                case '\t': *ptr2++ = 't'; break;
                default: snprintf(ptr2, 6, "u%04x", (unsigned int)*ptr); ptr2 += 5; break;
            }
        }
    }
    *ptr2++ = '\"'; *ptr2++ = 0;
    return out;
}

static char *print_string(const cJSON *item) { return print_string_ptr(item->valuestring); }

static const char *parse_value(cJSON *item, const char *value)
{
    if (!value) return 0;
    if (!strncmp(value, "null", 4)) { item->type = cJSON_NULL; return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4)) { item->type = cJSON_True; item->valueint = 1; return value + 4; }
    if (*value == '\"') { return parse_string(item, value); }
    if (*value == '-' || (*value >= '0' && *value <= '9')) { return parse_number(item, value); }
    if (*value == '[') { return parse_array(item, value); }
    if (*value == '{') { return parse_object(item, value); }

    global_ep = value; return 0;
}

static char *print_value(const cJSON *item, int depth, int fmt)
{
    char *out = 0;
    if (!item) return 0;
    switch ((item->type) & 255)
    {
        case cJSON_NULL: out = cJSON_strdup("null"); break;
        case cJSON_False: out = cJSON_strdup("false"); break;
        case cJSON_True: out = cJSON_strdup("true"); break;
        case cJSON_Number: out = print_number(item); break;
        case cJSON_String: out = print_string(item); break;
        case cJSON_Array: out = print_array(item, depth, fmt); break;
        case cJSON_Object: out = print_object(item, depth, fmt); break;
        case cJSON_Raw: out = cJSON_strdup(item->valuestring ? item->valuestring : ""); break;
    }
    return out;
}

static const char *parse_array(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '[') { global_ep = value; return 0; }

    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return 0;
    value = skip(parse_value(child, skip(value)));
    if (!value) return 0;

    while (*value == ',')
    {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return 0;
        child->next = new_item; new_item->prev = child; child = new_item;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return 0;
    }

    if (*value == ']') return value + 1;
    global_ep = value; return 0;
}

static char *print_array(const cJSON *item, int depth, int fmt)
{
    char **entries; char *out = 0, *ptr, *ret; size_t len = 5;
    cJSON *child = item->child;
    int numentries = 0, i = 0, fail = 0;

    while (child) numentries++, child = child->next;
    if (!numentries) {
        out = (char*)cJSON_malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }

    entries = (char**)cJSON_malloc(numentries * sizeof(char*));
    if (!entries) return 0;
    memset(entries, 0, numentries * sizeof(char*));

    child = item->child;
    while (child)
    {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;
        if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0); else { fail = 1; break; }
        child = child->next;
    }

    if (!fail) out = (char*)cJSON_malloc(len);
    if (!out) fail = 1;

    if (fail) {
        for (i = 0; i < numentries; i++) if (entries[i]) cJSON_free_fn(entries[i]);
        cJSON_free_fn(entries);
        return 0;
    }

    *out = '['; ptr = out + 1; *ptr = 0;
    for (i = 0; i < numentries; i++)
    {
        strcpy(ptr, entries[i]); ptr += strlen(entries[i]);
        if (i != numentries - 1) { *ptr++ = ','; if (fmt) *ptr++ = ' '; *ptr = 0; }
        cJSON_free_fn(entries[i]);
    }
    cJSON_free_fn(entries);
    *ptr++ = ']'; *ptr++ = 0;
    return out;
}

static const char *parse_object(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '{') { global_ep = value; return 0; }

    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return 0;
    value = skip(parse_string(child, skip(value)));
    if (!value) return 0;
    child->string = child->valuestring; child->valuestring = 0;
    if (*value != ':') { global_ep = value; return 0; }
    value = skip(parse_value(child, skip(value + 1)));
    if (!value) return 0;

    while (*value == ',')
    {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return 0;
        child->next = new_item; new_item->prev = child; child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value) return 0;
        child->string = child->valuestring; child->valuestring = 0;
        if (*value != ':') { global_ep = value; return 0; }
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return 0;
    }

    if (*value == '}') return value + 1;
    global_ep = value; return 0;
}

static char *print_object(const cJSON *item, int depth, int fmt)
{
    char **entries = 0, **names = 0;
    char *out = 0, *ptr, *ret, *str;
    size_t len = 7;
    int i = 0, numentries = 0, fail = 0;
    cJSON *child = item->child;

    while (child) numentries++, child = child->next;
    if (!numentries) {
        out = (char*)cJSON_malloc(fmt ? depth + 4 : 3);
        if (!out) return 0;
        strcpy(out, "{}");
        return out;
    }

    entries = (char**)cJSON_malloc(numentries * sizeof(char*));
    if (!entries) return 0;
    names = (char**)cJSON_malloc(numentries * sizeof(char*));
    if (!names) { cJSON_free_fn(entries); return 0; }
    memset(entries, 0, sizeof(char*) * numentries);
    memset(names, 0, sizeof(char*) * numentries);

    child = item->child; depth++;
    while (child)
    {
        names[i] = str = print_string_ptr(child->string);
        entries[i++] = ret = print_value(child, depth, fmt);
        if (str && ret) len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0); else { fail = 1; break; }
        child = child->next;
    }

    if (!fail) out = (char*)cJSON_malloc(len);
    if (!out) fail = 1;

    if (fail) {
        for (i = 0; i < numentries; i++) {
            if (names[i]) cJSON_free_fn(names[i]);
            if (entries[i]) cJSON_free_fn(entries[i]);
        }
        cJSON_free_fn(names); cJSON_free_fn(entries);
        return 0;
    }

    *out = '{'; ptr = out + 1; if (fmt) *ptr++ = '\n'; *ptr = 0;
    for (i = 0; i < numentries; i++)
    {
        if (fmt) for (int j = 0; j < depth; j++) *ptr++ = '\t';
        strcpy(ptr, names[i]); ptr += strlen(names[i]);
        *ptr++ = ':'; if (fmt) *ptr++ = '\t';
        strcpy(ptr, entries[i]); ptr += strlen(entries[i]);
        if (i != numentries - 1) *ptr++ = ',';
        if (fmt) *ptr++ = '\n'; *ptr = 0;
        cJSON_free_fn(names[i]); cJSON_free_fn(entries[i]);
    }
    cJSON_free_fn(names); cJSON_free_fn(entries);
    if (fmt) for (int i = 0; i < depth - 1; i++) *ptr++ = '\t';
    *ptr++ = '}'; *ptr++ = 0;
    return out;
}

int cJSON_GetArraySize(const cJSON *array) {
    cJSON *c = array ? array->child : 0; int i = 0;
    while (c) i++, c = c->next; return i;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    cJSON *c = array ? array->child : 0;
    while (c && index > 0) index--, c = c->next; return c;
}

cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string) {
    cJSON *c = object ? object->child : 0;
    while (c && strcasecmp(c->string, string)) c = c->next;
    return c;
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string) {
    cJSON *c = object ? object->child : 0;
    while (c && strcmp(c->string, string)) c = c->next;
    return c;
}

cJSON_bool cJSON_HasObjectItem(const cJSON *object, const char *string) {
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

cJSON_bool cJSON_IsInvalid(const cJSON * const item) { return (!item || item->type == cJSON_Invalid); }
cJSON_bool cJSON_IsFalse(const cJSON * const item) { return (item && (item->type & 255) == cJSON_False); }
cJSON_bool cJSON_IsTrue(const cJSON * const item) { return (item && (item->type & 255) == cJSON_True); }
cJSON_bool cJSON_IsBool(const cJSON * const item) { return (item && ((item->type & 255) == cJSON_True || (item->type & 255) == cJSON_False)); }
cJSON_bool cJSON_IsNull(const cJSON * const item) { return (item && (item->type & 255) == cJSON_NULL); }
cJSON_bool cJSON_IsNumber(const cJSON * const item) { return (item && (item->type & 255) == cJSON_Number); }
cJSON_bool cJSON_IsString(const cJSON * const item) { return (item && (item->type & 255) == cJSON_String); }
cJSON_bool cJSON_IsArray(const cJSON * const item) { return (item && (item->type & 255) == cJSON_Array); }
cJSON_bool cJSON_IsObject(const cJSON * const item) { return (item && (item->type & 255) == cJSON_Object); }
cJSON_bool cJSON_IsRaw(const cJSON * const item) { return (item && (item->type & 255) == cJSON_Raw); }

cJSON *cJSON_CreateNull(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_NULL; return item; }
cJSON *cJSON_CreateTrue(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_True; return item; }
cJSON *cJSON_CreateFalse(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_False; return item; }
cJSON *cJSON_CreateBool(cJSON_bool boolean) { cJSON *item = cJSON_New_Item(); if (item) item->type = boolean ? cJSON_True : cJSON_False; return item; }
cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (item) { item->type = cJSON_Number; item->valuedouble = num; item->valueint = (int)num; }
    return item;
}
cJSON *cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (item) { item->type = cJSON_String; item->valuestring = cJSON_strdup(string); }
    return item;
}
cJSON *cJSON_CreateRaw(const char *raw) {
    cJSON *item = cJSON_New_Item();
    if (item) { item->type = cJSON_Raw; item->valuestring = cJSON_strdup(raw); }
    return item;
}
cJSON *cJSON_CreateArray(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Array; return item; }
cJSON *cJSON_CreateObject(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Object; return item; }

cJSON_bool cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    cJSON *c;
    if (!item) return 0;
    if (!array) { cJSON_Delete(item); return 0; }
    c = array->child;
    if (!c) { array->child = item; }
    else { while (c && c->next) c = c->next; c->next = item; item->prev = c; }
    return 1;
}

cJSON_bool cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!item) return 0;
    if (!object) { cJSON_Delete(item); return 0; }
    if (item->string) cJSON_free_fn(item->string);
    item->string = cJSON_strdup(string);
    return cJSON_AddItemToArray(object, item);
}

cJSON_bool cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item) {
    return cJSON_AddItemToObject(object, string, item);
}

cJSON *cJSON_DetachItemFromArray(cJSON *array, int which) {
    cJSON *c = array ? array->child : 0;
    while (c && which > 0) c = c->next, which--;
    if (!c) return 0;
    if (c->prev) c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
    if (c == array->child) array->child = c->next;
    c->prev = c->next = 0;
    return c;
}

void cJSON_DeleteItemFromArray(cJSON *array, int which) { cJSON_Delete(cJSON_DetachItemFromArray(array, which)); }

cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string) {
    int i = 0; cJSON *c = object ? object->child : 0;
    while (c && strcasecmp(c->string, string)) i++, c = c->next;
    if (c) return cJSON_DetachItemFromArray(object, i);
    return 0;
}

void cJSON_DeleteItemFromObject(cJSON *object, const char *string) { cJSON_Delete(cJSON_DetachItemFromObject(object, string)); }

cJSON *cJSON_AddNullToObject(cJSON * const object, const char * const name) {
    cJSON *null = cJSON_CreateNull(); cJSON_AddItemToObject(object, name, null); return null;
}
cJSON *cJSON_AddTrueToObject(cJSON * const object, const char * const name) {
    cJSON *b = cJSON_CreateTrue(); cJSON_AddItemToObject(object, name, b); return b;
}
cJSON *cJSON_AddFalseToObject(cJSON * const object, const char * const name) {
    cJSON *b = cJSON_CreateFalse(); cJSON_AddItemToObject(object, name, b); return b;
}
cJSON *cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean) {
    cJSON *b = cJSON_CreateBool(boolean); cJSON_AddItemToObject(object, name, b); return b;
}
cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number) {
    cJSON *num = cJSON_CreateNumber(number); cJSON_AddItemToObject(object, name, num); return num;
}
cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string) {
    cJSON *str = cJSON_CreateString(string); cJSON_AddItemToObject(object, name, str); return str;
}
cJSON *cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw) {
    cJSON *r = cJSON_CreateRaw(raw); cJSON_AddItemToObject(object, name, r); return r;
}
cJSON *cJSON_AddObjectToObject(cJSON * const object, const char * const name) {
    cJSON *obj = cJSON_CreateObject(); cJSON_AddItemToObject(object, name, obj); return obj;
}
cJSON *cJSON_AddArrayToObject(cJSON * const object, const char * const name) {
    cJSON *arr = cJSON_CreateArray(); cJSON_AddItemToObject(object, name, arr); return arr;
}
