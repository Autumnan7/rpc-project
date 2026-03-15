#pragma once
#include <cstddef>

namespace minico
{
    // 编译期固定参数
    namespace parameter
    {
        // const:常量，值不可修改
        // static:静态，类内静态成员变量，属于类而不是对象
        // size_t:无符号整数类型，通常用于表示大小或计数
        const static size_t coroutineStackSize = 8 * 1024;

        // constexpr:编译时常量，值在编译时确定，可以用于数组大小等需要编译时常量的场景
        //  epollerListFirstSize:epoll事件列表的初始大小，表示在第一次调用epoll_wait时分配的事件列表的大小。
        static constexpr int epollerListFirstSize = 16;

        // epollTimeOutMs:epoll_wait的超时时间，单位为毫秒，-1表示无限等待，直到有事件发生。
        static constexpr int epollTimeOutMs = -1;

        // backLog:listen函数的backlog参数，表示服务器套接字在完成三次握手后可以排队的最大连接数。
        constexpr static unsigned backLog = 4096;

        // memPoolMallocObjCnt:内存池分配对象的数量，表示每次从内存池分配的对象数量。
        static constexpr size_t memPoolMallocObjCnt = 40;
    }

}