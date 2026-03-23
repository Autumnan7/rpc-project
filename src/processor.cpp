#include "processor.h"
#include "parameter.h"
#include "spinlock_guard.h"

#include <sys/epoll.h>
#include <unistd.h>

using namespace minico;

thread_local int threadIdx = -1;

Processor::Processor(int tid)
    : threadId_(tid),
      state_(ProcessorStatus::Stopped),
      workerThread_(),
      activePendingQueue_(0),
      currentCoroutine_(nullptr),
      // TODO: 参考用的是0
      schedulerContext_(parameter::coroutineStackSize)
{
    // 初始化处理器主程序上下文
    schedulerContext_.makeCurContext();
}

Processor::~Processor()
{
    // 如果还在运行，先请求停止
    if (state_ == ProcessorStatus::Running)
    {
        stop();
    }

    // // 如果已经进入停止中状态，等待线程退出
    // if (state_ == ProcessorStatus::Stopping)
    // {
    //     join();
    // }

    // // 清理工作线程对象
    // if (workerThread_.joinable())
    // {
    //     workerThread_.join();
    // }

    join();

    // 释放所有仍由 Processor 管理的协程对象
    for (auto *co : allCoroutines_)
    {
        delete co;
    }
    allCoroutines_.clear();
}

void Processor::resumeCoroutine(Coroutine *co)
{
    // 空指针直接返回
    if (co == nullptr)
    {
        return;
    }

    // 只允许恢复当前 Processor 管理的协程
    if (allCoroutines_.find(co) == allCoroutines_.end())
    {
        return;
    }

    // 记录当前正在运行的协程
    currentCoroutine_ = co;

    // 恢复协程执行
    co->resume();
}

// TODO：什么时候会为空呢
void Processor::yield()
{
    Coroutine *co = currentCoroutine_;
    if (co == nullptr)
    {
        return;
    }

    co->yield();
    schedulerContext_.swapToMe(co->getCtx());
}

void Processor::wait(Time time)
{
    Coroutine *co = currentCoroutine_;
    if (co == nullptr)
    {
        return;
    }

    co->yield();
    timer_.runAfter(time, co);
    schedulerContext_.swapToMe(co->getCtx());
}

void Processor::goCo(Coroutine *co)
{
    if (co == nullptr)
    {
        return;
    }

    {
        SpinlockGuard lock(pendingQueueLock_);
        pendingCoroutines_[!activePendingQueue_].push(co);
    }

    // 每加入一个新协程就唤醒处理器，从而尽快执行该协程
    wakeEpoller();
}

void Processor::goCoBatch(const std::vector<Coroutine *> &cos)
{
    {
        SpinlockGuard lock(pendingQueueLock_);
        for (auto *co : cos)
        {
            if (co == nullptr)
            {
                continue;
            }
            pendingCoroutines_[!activePendingQueue_].push(co);
        }
    }

    wakeEpoller();
}

bool Processor::loop()
{

    if (!timer_.init(&eventPoller_))
    {
        return false;
    }

    // 把当前对象的 this 指针捕获进来，让 lambda 里面能访问成员变量和成员函数。
    workerThread_ = std::thread([this]
                                {
            threadIdx = threadId_;
            state_ = ProcessorStatus::Running;
            while (state_== ProcessorStatus::Running)
            {
                if (!readyCoroutines_.empty())
                {
                    readyCoroutines_.clear();
                }


                if (!timeoutCoroutines_.empty())
                {
                    timeoutCoroutines_.clear();
                }
 

                // 获取活跃的事件，这里是loop中唯一会被阻塞的地方(epoll_wait)
                eventPoller_.poll(parameter::epollTimeOutMs, readyCoroutines_);

                // 1. 处理定时任务协程
                timer_.getExpiredCoroutines(timeoutCoroutines_);

                for (auto *co: timeoutCoroutines_)
                {
                    resumeCoroutine(co);
                }


                // 2. 执行新加入的协程，按 FIFO 顺序把 pending 队列里的协程交给调度器执行
                // runningQueue 是当前正在被主循环消费的 pending 队列下标，取值为 0 或 1
                // pendingCoroutines_[runningQueue] 是当前正在被主循环消费的 pending 队列
                int runningQueue = activePendingQueue_.load(); // load：从原子变量里安全地读出当前值
                while(!pendingCoroutines_[runningQueue].empty())
                {
                    Coroutine* co = pendingCoroutines_[runningQueue].front();
                    pendingCoroutines_[runningQueue].pop();
                    allCoroutines_.insert(co);
                    resumeCoroutine(co);
                }

                {
                    // 上锁并转换任务队列
                    SpinlockGuard lock(pendingQueueLock_);
                    activePendingQueue_ = !runningQueue;
                }

                // 3. 执行被epoll唤醒的协程

                for(auto* co: readyCoroutines_)
                {
                    resumeCoroutine(co);
                }

                // 4. 销毁已经执行完毕的协程
                for (auto *deadCo : cleanupCoroutines_)
                {
                    allCoroutines_.erase(deadCo);
                    // delete deadCo;
                    {
                        SpinlockGuard lock(coroutinePoolLock_);
                        coroutinePool_.delete_obj(deadCo);
                    }
                }
                cleanupCoroutines_.clear();
            }
            // 如果跳出循环 状态变更为处理器暂停
            state_ = ProcessorStatus::Stopped; });
    return true;
}

// 当前协程等待 fd 上的某个事件，先注册监听，再主动让出执行权，恢复后再移除监听。
// addEvent -> yield -> resume -> removeEvent
void Processor::waitEvent(int fd, int ev)
{
    eventPoller_.addEvent(currentCoroutine_, fd, ev);
    yield();
    eventPoller_.removeEvent(currentCoroutine_, fd, ev);
}

void Processor::stop()
{
    state_ = ProcessorStatus::Stopping;
    wakeEpoller();
}

void Processor::join()
{
    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}

void Processor::wakeEpoller()
{
    timer_.wakeUp();
}

void Processor::killCurCo()
{
    cleanupCoroutines_.push_back(currentCoroutine_);
}
