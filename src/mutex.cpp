#include "../include/mutex.h"
#include "../include/scheduler.h"
#include "../include/spinlock_guard.h"

using namespace minico;

void RWMutex::rlock()
{
    {
        SpinlockGuard l(lock_);
        if (state_ == MU_FREE || state_ == MU_READING)
        {
            readingNum_.fetch_add(1);
            state_ = MU_READING;
            return;
        }
        // 如果是写锁持有中，则当前协程需要等待，加入等待队列
        /** 相当于互斥了 就将当前没有上读锁的协程放入协程队列*/
        waitingCo_.push(Scheduler::getScheduler()->getProcessor(threadIdx)->getCurRunningCo());
    }
    Scheduler::getScheduler()->getProcessor(threadIdx)->yield();
    // 这个函数第一次可能不会执行完，但协程会在 yield() 后暂停，等被唤醒时继续往下跑，直到真正拿到锁为止
    rlock();
}

void RWMutex::runlock()
{
    SpinlockGuard l(lock_);
    auto cur = readingNum_.fetch_sub(1); // fetch_add 原子操作
    if (cur == 1)
    {
        freeLock();
    }
}

void RWMutex::wlock()
{
    {
        SpinlockGuard l(lock_);
        if (state_ == MU_FREE)
        {
            state_ = MU_WRITING;
            return;
        }
        waitingCo_.push(Scheduler::getScheduler()->getProcessor(threadIdx)->getCurRunningCo());
    }
    Scheduler::getScheduler()->getProcessor(threadIdx)->yield();
    wlock();
}
void RWMutex::wunlock()
{
    SpinlockGuard l(lock_);
    freeLock();
}

void RWMutex::freeLock()
{
    state_ = MU_FREE;
    while (!waitingCo_.empty())
    {
        auto wakeCo = waitingCo_.front();
        waitingCo_.pop();
        wakeCo->getOwnerProcessor()->goCo(wakeCo);
    }
}