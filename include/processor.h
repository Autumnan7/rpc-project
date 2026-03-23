#pragma once
#include <queue>
#include <set>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>

#include "objpool.h"
#include "spinlock.h"
#include "context.h"
#include "coroutine.h"
#include "epoller.h"
#include "timer.h"
#include "logger.h"

/**
 * @brief 协程调度器
 *
 * @details
 * Processor 表示一个运行在独立线程上的协程调度器，负责管理该线程上的所有协程对象，
 * 并驱动协程在以下几类事件下恢复执行：
 * 1. 新协程加入
 * 2. epoll 事件触发
 * 3. 定时器超时
 * 4. 协程主动让出、等待或退出
 *
 * 调度器内部采用双缓冲新协程队列，以减少并发写入与主循环消费之间的冲突。
 * 主循环会按固定顺序处理任务：
 * timer -> pending(new) -> ready(epoll) -> cleanup
 *
 * 主要职责：
 * - 接收并调度新协程
 * - 维护协程生命周期
 * - 处理 IO 事件与定时事件
 * - 提供协程切换、挂起、唤醒、销毁能力
 */

extern thread_local int threadIdx;

namespace minico
{

    enum class ProcessorStatus
    {
        Running = 0,
        Stopping,
        Stopped
    };

    // 新协程是否正在加入双缓冲队列
    enum class NewCoroutineState
    {
        Adding,
        Added
    };

    class Processor
    {
    public:
        /** 构造一个调度器实例，tid 表示线程编号 */
        explicit Processor(int tid);

        /** 析构调度器，负责释放资源 */
        ~Processor();

        /** 禁止拷贝、移动和赋值，避免线程/上下文被错误复制 */
        DISALLOW_COPY_MOVE_AND_ASSIGN(Processor);

        // =========================
        // 协程创建与调度
        // =========================

        /**
         * @brief 创建并调度一个新的协程
         * @tparam F 协程入口函数的类型
         * @param coFunc 协程入口函数
         * @param stackSize 协程栈大小
         */
        template <typename F>
        void Processor::goNewCo(F &&coFunc, size_t stackSize)
        {
            Coroutine *pCo = nullptr;
            {
                SpinlockGuard lock(coPoolLock_);
                pCo = coPool_.new_obj(this, stackSize, std::forward<F>(coFunc));
            }
            goCo(pCo);
        }

        /** 调度一个已经存在的协程对象 */
        void goCo(Coroutine *co);

        /** 批量调度一组已存在的协程对象 */
        void goCoBatch(const std::vector<Coroutine *> &cos);

        // =========================
        // 协程运行控制
        // =========================

        /** 当前协程主动让出执行权 */
        void yield();

        /** 当前协程等待指定时间，单位为毫秒 */
        void wait(Time time);

        /** 销毁当前正在运行的协程 */
        void killCurCo();

        /** 等待 fd 上发生 ev 事件 */
        void waitEvent(int fd, int ev);

        // =========================
        // 主循环与线程控制
        // =========================

        /** 运行处理器的主事件循环 */
        bool loop();

        /** 请求停止线程循环 */
        void stop();

        /** 等待线程退出 */
        void join();

        // =========================
        // 查询接口
        // =========================

        /** 获取当前正在运行的协程 */
        Coroutine *getCurRunningCo() const { return currentCoroutine_; }

        /** 获取调度器主上下文 */
        Context *getMainCtx() { return &schedulerContext_; }

        /** 获取当前调度器管理的协程总数 */
        std::size_t getCoCnt() const { return allCoroutines_.size(); }

    private:
        // =========================
        // 核心运行控制
        // =========================

        /** 恢复指定协程的执行 */
        void resumeCoroutine(Coroutine *co);

        /** 唤醒阻塞在epoll的线程 */
        void wakeEpoller();

        /** 当前处理器所属线程编号 */
        int threadId_;

        /** 当前调度器状态 */
        ProcessorStatus state_;

        /** 调度器对应的工作线程 */
        std::thread workerThread_;

        // =========================
        // 新协程加入管理
        // =========================

        /**
         * @brief 待调度协程队列（双缓冲）
         * @note 两个队列交替使用，实现生产者写入与消费者读取的解耦，减少锁竞争。
         */
        std::queue<Coroutine *> pendingCoroutines_[2];

        /**
         * @brief 当前活跃队列索引
         * @note 原子变量，值为0或1。生产者写入非活跃队列，消费者取出活跃队列。
         */
        std::atomic<int> activePendingQueue_{0};

        /** 保护 pending 队列切换与写入的锁 */
        Spinlock pendingQueueLock_;

        // =========================
        // 协程生命周期管理
        // =========================

        /** 保护协程对象池的锁 */
        Spinlock coroutinePoolLock_;

        /** 当前调度器管理的所有协程集合，用于生命周期跟踪 */
        std::set<Coroutine *> allCoroutines_;

        /** 因定时器超时而重新变为可执行的协程队列 */
        std::vector<Coroutine *> timeoutCoroutines_;

        /** 已结束、等待统一销毁的协程队列 */
        std::vector<Coroutine *> cleanupCoroutines_;

        /** 协程对象池，用于复用 Coroutine 对象 */
        ObjPool<Coroutine> coroutinePool_;

        // =========================
        // 事件驱动与调度上下文
        // =========================

        /** 已经可以继续执行的协程队列（来自 epoll 激活） */
        std::vector<Coroutine *> readyCoroutines_;

        /** epoll 事件轮询器 */
        Epoller eventPoller_;

        /** 定时器管理器 */
        Timer timer_;

        /** 当前正在运行的协程 */
        Coroutine *currentCoroutine_;

        /** 调度器主上下文，用于协程切换 */
        Context schedulerContext_;
    };

}
