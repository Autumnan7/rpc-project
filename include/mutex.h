#pragma once
#include "coroutine.h"
#include "spinlock.h"

#include <atomic>
#include <queue>

namespace minico
{
    // 读写锁状态
    enum muStatus
    {
        MU_FREE = 0, // 空闲
        MU_READING,  // 读锁持有中
        MU_WRITING   // 写锁持有中
    };

    /** 用于协程同步的读写锁：
     *  读锁之间不互斥，读锁与写锁互斥，写锁与写锁互斥。
     */
    class RWMutex
    {
    public:
        RWMutex()
            : state_(MU_FREE), readingNum_(0) {};
        ~RWMutex() {};

        DISALLOW_COPY_MOVE_AND_ASSIGN(RWMutex);

        void rlock();   // 获取读锁
        void runlock(); // 释放读锁
        void wlock();   // 获取写锁
        void wunlock(); // 释放写锁

    private:
        void freeLock(); // 锁空闲时，唤醒等待协程

        int state_;                         // 当前锁状态
        std::atomic_int readingNum_;        // 当前读者数量
        Spinlock lock_;                     // 保护共享状态的自旋锁
        std::queue<Coroutine *> waitingCo_; // 等待获取锁的协程队列
    };
}