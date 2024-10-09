/**
 * @file leptjson.h
 * @author nicejzy
 * @brief parser json type of null and boolean
 * @version 0.1
 * @date 2024-09-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h>     /* size_t */

typedef enum {
    LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;


typedef struct {    
    /* 一个值不可能同时为数字和字符串，因此我们可使用 C 语言的 union 来节省内存 */
    union {
        struct { char* s; size_t len; } s;      /* string */
        double n;                               /* number */
    } u;
    
    lept_type type;
} lept_value;


enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_NUMBER_TOO_BIG,
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_ESCAPE,
    LEPT_PARSE_INVALID_STRING_CHAR
};

/* 定义了一个宏 lept_init，用于初始化 lept_value 结构体，将其类型设置为 LEPT_NULL */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)

int lept_parse(lept_value* v, const char* json);

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);

/* 将 lept_value 设置为 null，通过调用 lept_free 释放内存 */
#define lept_set_null(v) lept_free(v)

/* 声明了获取和设置布尔值的函数 */
int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

/* 声明了获取和设置数字的函数 */
double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

/* 声明了获取字符串、获取字符串长度以及设置字符串的函数 */
const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

#endif