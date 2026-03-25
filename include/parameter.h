#pragma once
#include <cstddef>

namespace minico
{
    // 编译期固定参数
    namespace parameter
    {

        // 协程栈大小，单位为字节
        const static size_t coroutineStackSize = 8 * 1024;
        // const:常量，值不可修改
        // static:静态，类内静态成员变量，属于类而不是对象
        // size_t:无符号整数类型，通常用于表示大小或计数

        // epoll事件列表的初始大小，表示在第一次调用epoll_wait时分配的事件列表的大小。
        static constexpr int epollerListFirstSize = 16;
        // constexpr：编译期常量，通常用于表示不会变化、且需要在编译期就确定的值

        // epoll_wait 的超时时间，单位为毫秒，-1表示无限等待，直到有事件发生。
        static constexpr int epollTimeOutMs = -1;

        // listen 的 backlog 参数，表示已完成三次握手、等待被 accept 的最大连接数
        constexpr static unsigned backLog = 4096;

        // 内存池每次向系统申请的对象块数量
        static constexpr size_t memPoolMallocObjCnt = 40;
    }

}