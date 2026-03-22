#include "../include/epoller.h"
#include "../include/coroutine.h"
#include "../include/parameter.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

namespace minico
{

    // 构造函数：直接初始化 epoll 实例
    Epoller::Epoller()
        : epollFd_(-1),
          activeEpollEvents_(parameter::epollerListFirstSize)
    {
        // 直接在构造函数中创建 epoll 实例，无需 init()
        epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    }

    // 析构函数：RAII 风格，自动关闭句柄
    Epoller::~Epoller()
    {
        if (isValid())
        {
            ::close(epollFd_);
        }
    }

    // 添加监听事件
    bool Epoller::addEvent(Coroutine *pCo, int fd, int events)
    {
        if (!isValid())
        {
            return false;
        }
        epoll_event event{static_cast<uint32_t>(events), pCo};

        return ::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) == 0;
    }

    // 修改监听事件
    bool Epoller::modifyEvent(Coroutine *pCo, int fd, int events)
    {
        if (!isValid())
        {
            return false;
        }
        epoll_event event{static_cast<uint32_t>(events), pCo};
        return ::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) == 0;
    }

    // 移除监听事件
    bool Epoller::removeEvent(Coroutine *pCo, int fd, int events)
    {
        if (!isValid())
        {
            return false;
        }
        // 注意：在 Linux 上，EPOLL_CTL_DEL 忽略 event 参数，
        // 但为了接口统一和跨平台考虑，这里依然填充结构体。
        epoll_event event{static_cast<uint32_t>(events), pCo};
        return ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event) == 0;
    }

    // 等待事件发生
    int Epoller::poll(int timeoutMs, std::vector<Coroutine *> &activeCoroutines)
    {
        if (!isValid())
        {
            return -1;
        }

        // 清空输出容器，防止上次结果污染
        activeCoroutines.clear();

        int actEvNum = ::epoll_wait(epollFd_,
                                    &*activeEpollEvents_.begin(),
                                    static_cast<int>(activeEpollEvents_.size()),
                                    timeoutMs);
        // 错误处理
        if (actEvNum < 0)
        {
            int savedErrno = errno; // 先保存现场
            if (savedErrno == EINTR)
            {
                // 被信号中断，不算错误，返回 0 表示无事件
                return 0;
            }
            // 其他真实错误，打印日志并返回 -1
            return -1;
        }

        if (actEvNum > 0)
        {
            // 遍历所有激活的事件
            for (int i = 0; i < actEvNum; ++i)
            {
                // 从 data.ptr 取出之前存入的 Coroutine*
                Coroutine *pCo = static_cast<Coroutine *>(activeEpollEvents_[i].data.ptr);

                activeCoroutines.push_back(pCo);
            }

            // 动态扩容：如果事件数量填满了缓冲区，下次可能不够用，进行扩容
            if (static_cast<size_t>(actEvNum) == activeEpollEvents_.size())
            {
                activeEpollEvents_.resize(activeEpollEvents_.size() * 2);
            }
        }

        // 返回激活的事件数量
        return actEvNum;
    }

} // namespace minico
