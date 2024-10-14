#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h> 
#endif
#include "leptjson.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>      /* errno, ERANGE */
#include <math.h>       /* HUGE_VAL */
#include <stdlib.h>     /* NULL, malloc, free, realloc, strtod */
#include <string.h>     /* memcpy() */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

/* 确保当前 JSON 字符等于预期的字符 ch，并将 JSON 指针向前移动一个字符 */
#define EXPECT(c, ch)   do { assert(*c->json == (ch)); c->json++; } while(0)

#define ISDIGIT(ch)     ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

/* 将字符 ch 存入上下文栈中 */
#define PUTC(c, ch)        do { *(char*)lept_context_push(c, sizeof(char)) == (ch); } while(0)

/**
 * @brief 为了减少解析函数之间传递多个参数，
 *        把这些数据都放进一个 lept_context 结构体。
 * 
 * lept_context 结构体用于存储解析过程中的上下文信息，
 * 包括当前 JSON 字符串指针、栈、栈的大小和栈顶位置。
 */
typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
} lept_context;

/**
 * @brief 将指定大小的内存推入栈中。如果当前栈的大小不足以容纳新数据，就会扩展栈的大小。
 * 
 * @param c 
 * @param size 
 * @return void* 新推入位置的指针
 */
static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if(c->top + size >= c->size) {
        if(c->size == 0) {
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        }
        while(c->top + size >= c->size) {
            c->size += c->size >> 1;    /* c->size * 1.5 */
        }
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

/**
 * @brief 从栈中弹出指定大小的内存，并更新栈顶位置
 * 
 * @param c 
 * @param size 
 * @return void* 
 */
static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

/**
 * @brief 跳过 JSON 字符串中的空白字符（空格、换行符、回车和制表符）
 * 
 * @param c 
 */
static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
        ++p;
    c->json = p;
}

/**
 * @brief 解析字面量 true、false、null。检查当前 JSON 字符串是否匹配给定的字面量，并将其类型设置为相应的枚举类型
 * 
 * @param c 
 * @param v 
 * @param literal 
 * @param type 
 * @return int 
 */
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for(i = 0; literal[i + 1]; i++) {
        if(c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

/**
 * @brief 解析 JSON 中的数字。支持负数、浮点数和科学计数法。如果解析成功，将结果存入 lept_value 结构体中
 * 
 * @param c 
 * @param v 
 * @return int 
 */
static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;
    if(*p == '-') p++;
    if(*p == '0') p++;
    else {
        if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++; ISDIGIT(*p); p++);
    }
    if(*p == '.') {
        p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++; ISDIGIT(*p); p++);
    }
    if(*p == 'e' || *p == 'E') {
        p++;
        if(*p == '+' || *p == '-') p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++; ISDIGIT(*p); p++);
    } 
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if(errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}


/**
 * @brief 将一个包含4个十六进制数字的字符串转换为一个无符号整数，并返回指向字符串下一个字符的指针
 * @param p
 */
static const char* lept_parse_hex4(const char* p, unsigned* u) {
    int i;
    *u = 0;
    for(i = 0; i < 4; ++i) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9') *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F') *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f') *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

/**
 * @brief 将一个无符号整数（表示Unicode代码点）编码为UTF-8格式，并将结果存储在给定的 lept_context 结构体中
 *        它的实现遵循UTF-8编码规则，并根据代码点的范围决定使用多少个字节进行编码
 * @param c
 * @param u
 */
static void lept_encode_utf8(lept_context* c, unsigned u) {
    if(u <= 0x7F)                               // 单字节编码（U+0000 到 U+007F）
        PUTC(c, u & 0xFF);
    else if(u <= 0x7FF) {                       // 双字节编码（U+0080 到 U+07FF）
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if(u <= 0xFFFF) {                      // 三字节编码（U+0800 到 U+FFFF）
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {                                      // 四字节编码（U+10000 到 U+10FFFF）
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

/**
 * @brief 解析 JSON 中的字符串，处理转义字符和错误情况（如缺失引号、无效字符等）。如果成功解析字符串，则更新 lept_value
 * 
 * @param c 
 * @param v 
 * @return int 
 */
static int lept_parse_string(lept_context* c, lept_value* v) {
    size_t head = c->top, len;
    unsigned u, u2;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for(;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;

            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        if(!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if(u >= 0xD800 && u <= 0xDBFF) {    /* surrogate pair */
                            if(*p++ != '\\')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if(*p++ != 'u')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if(!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if(u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            
            default:
                if((unsigned char)ch < 0x20) {
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch);
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v);


static int lept_parse_array(lept_context* c, lept_value* v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    if(*c->json == ']') {
        c->json++;
        v->type = LEPT_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return LEPT_PARSE_OK;
    }

    for(;;) {
        lept_value e;
        lept_init(&e);
        if(ret = lept_parse_value(c, &e) != LEPT_PARSE_OK) 
            break;

        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
        size++;
        lept_parse_whitespace(c);
        if(*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        } else if(*c->json == ']') {
            c->json++;
            v->type = LEPT_ARRAY;
            v->u.a.size = size;
            size *= sizeof(lept_value);
            memcpy(v->u.a.e = (lept_value*)malloc(size), lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        } else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }

    /* Pop and free values on the stack */
    for(int i = 0; i < size; i++) {
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
    }
    return ret;
}


/**
 * @brief 根据当前 JSON 字符来决定解析的类型（布尔值、null、数字或字符串）。调用相应的解析函数
 * 
 * @param c 
 * @param v 
 * @return int 
 */
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch(*c->json) {
        case 't' : return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f' : return lept_parse_false(c, v, "false", LEPT_FALSE);
        case 'n' : return lept_parse_null(c, v, "null", LEPT_NULL);
        default  : return lept_parse_number(c, v);
        case '"' : return lept_parse_string(c, v); 
        case '\0' : return LEPT_PARSE_EXPECT_VALUE;
    }
}

/**
 * @brief 解析 JSON 字符串的函数。主解析函数，初始化上下文，调用解析方法并进行错误处理。确保 JSON 字符串解析结束后没有多余字符
 * 
 * @param v  
 * @param json 字符串
 * @return int 表示解析结果
 */
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    // v->type = LEPT_NULL;
    lept_init(v);
    lept_parse_whitespace(&c);

    if((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if(*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    fre(c.stack);
    return ret;
}

/**
 * @brief 释放 lept_value 结构体中字符串的内存，确保避免内存泄漏
 * 
 * @param v 
 */
void lept_free(lept_value* v) {
    assert(v != NULL);
    if(v->type == LEPT_STRING) {
        free(v->u.s.s);
    }
    v->type = LEPT_NULL;
}

/**
 * @brief 获取 lept_value 类型的函数
 * 
 * @param v 
 * @return lept_type 
 */
lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

size_t lept_get_array_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}


lept_value* lept_get_array_element(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}