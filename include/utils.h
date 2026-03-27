#pragma once

// 禁止拷贝和移动构造函数以及赋值运算符的宏定义
#define DISALLOW_COPY_MOVE_AND_ASSIGN(TypeName)     \
    TypeName(const TypeName &) = delete;            \
    TypeName(TypeName &&) = delete;                 \
    TypeName &operator=(const TypeName &) = delete; \
    TypeName &operator=(TypeName &&) = delete

// 要求以空行结尾