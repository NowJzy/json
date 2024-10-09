cmake_minimum_required (VERSION 2.6)
project (leptjson_test C)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -pedantic -Wall")
endif()

add_library(leptjson leptjson.c)
add_executable(leptjson_test test.c)

# 将 leptjson 库链接到 leptjson_test 可执行文件上。
# 这意味着在编译 leptjson_test 时，链接器会链接生成的 leptjson 库，
# 这样 leptjson_test 就可以使用 leptjson 库中定义的函数和数据结构
target_link_libraries(leptjson_test leptjson)