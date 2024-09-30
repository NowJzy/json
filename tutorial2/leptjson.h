/**
 * @file leptjson.h
 * @author nicejzy
 * @brief parser json type of null and boolean
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef LEPTJSON_H__
#define LEPTJSON_H__

typedef enum {
    LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;


typedef struct {
    lept_type type;
} lept_value;


enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR
};


int lept_parse(lept_value* v, const char* json);

lept_type lept_get_type(const lept_value* v);

#endif