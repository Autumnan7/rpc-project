#pragma once

#include "utils.h"
#include <vector>
#include <sys/epoll.h>
namespace minico
{
    class Coroutine;

    /**
     * @brief Epoll封装类
     * 负责监听文件描述符事件，并将事件映射为激活的协程
     */
    class Epoller
    {
    public:
        Epoller();
        ~Epoller();

        DISALLOW_COPY_MOVE_AND_ASSIGN(Epoller);

        /**
         * @brief 修改监听事件
         * @param pCo 关联的协程对象
         * @param fd 文件描述符
         * @param events 关注的事件类型 (如 EPOLLIN, EPOLLOUT)
         */
        bool modifyEvent(Coroutine *pCo, int fd, int events);

        /**
         * @brief 添加监听事件
         */
        bool addEvent(Coroutine *pCo, int fd, int events);

        /**
         * @brief 移除监听事件
         */
        bool removeEvent(Coroutine *pCo, int fd, int events);

        /**
         * @brief 等待事件发生，轮询获取活跃事件
         * @param timeoutMs 超时时间(毫秒)
         * @param activeCoroutines [out] 返回激活的协程列表
         * @return 激活的事件数量，-1表示出错
         */
        int poll(int timeoutMs, std::vector<Coroutine *> &activeCoroutines);

    private:
        /**
         * @brief 判断Epoll句柄是否有效
         * @note 使用 const 修饰，表示不修改成员变量
         */
        inline bool isValid() const { return epollFd_ >= 0; }

        /** 内部epoll句柄 */
        int epollFd_;

        /** 用于存储返回事件的缓冲区 */
        std::vector<struct epoll_event> activeEpollEvents_;
    };
}
