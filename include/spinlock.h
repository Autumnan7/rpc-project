#pragma once
#include <atomic>
#include "utils.h"

namespace minico
{

    /**
     * @brief 基于原子操作的自旋锁（Spinlock）
     *
     * 使用 std::atomic_int 实现二元信号量：
     *  - 1 表示资源可用
     *  - 0 表示资源被占用
     */
    class Spinlock
    {
    public:
        Spinlock() : sem_(1) {} // 初始为可用

        ~Spinlock() { unlock(); }

        // 禁止拷贝、移动和赋值
        DISALLOW_COPY_MOVE_AND_ASSIGN(Spinlock);

        /**
         * @brief 获取锁
         * 如果锁已被占用（sem_==0），线程会自旋等待
         * 直到锁可用（sem_==1）再将其置为0，成功获取锁
         */
        void lock()
        {
            int expected = 1;
            // compare_exchange_strong:
            // 如果 sem_ == expected，则将 sem_ 置为 0，并返回 true
            // 否则，将 sem_ 的值写回 expected，并返回 false
            while (!sem_.compare_exchange_strong(expected, 0))
            {
                expected = 1; // 必须重置 expected 为 1，再尝试下一次交换
            }
        }

        /**
         * @brief 释放锁
         * 将信号量置为 1，表示资源可用
         */
        void unlock()
        {
            sem_.store(1, std::memory_order_release); // 明确释放语义
        }

    private:
        std::atomic_int sem_; ///< 二元信号量：1=可用，0=占用
    };

} // namespace minico