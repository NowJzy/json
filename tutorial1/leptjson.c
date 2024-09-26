#include "leptjson.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/**
 * @brief 为了减少解析函数之间传递多个参数，
 *        把这些数据都放进一个 lept_context 结构体
 */
typedef struct {
    const char* json;
} lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
        ++p;
    c->json = p;
}


static int lept_parse_value(lept_context* c, lept_value* v) {
    
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    return lept_parse_value(&c, v);
}

lept_type lept_get_type(const lept_value* v) {

}