#include "coroutine.h"
#include "epoller.h"
#include "timer.h"
#include "mstime.h"
#include <iostream>
#include <queue>
#include <vector>

// ==================== 全局资源 ====================
Epoller epoller;
Timer timer_mgr;                     // 假设你的定时器类叫 TimerManager
std::queue<Coroutine *> ready_queue; // 就绪队列

// ==================== 协程逻辑 ====================
void coroutine_func()
{
    std::cout << "[协程] 开始运行，当前时间: " << Time::now() << " ms\n";

    // 1. 获取当前协程指针 (这需要你的 Coroutine 类支持静态方法获取当前协程)
    Coroutine *current_co = Coroutine::GetCurrent();

    std::cout << "[协程] 准备睡 2 秒...\n";

    // 2. 添加定时器：2000ms 后，把协程塞回就绪队列
    timer_mgr.add_timer(2000, [current_co]()
                        {
        std::cout << "[定时器] 2秒到了，唤醒协程！" << std::endl;
        ready_queue.push(current_co); });

    // 3. 让出 CPU，切回主线程
    // 此时协程挂起，主线程会从 ready_queue 把它拿出来
    current_co->yield();

    // 4. --- 被唤醒后从这里继续 ---
    std::cout << "[协程] 睡醒了！恢复运行，当前时间: " << Time::now() << " ms\n";
}

// ==================== 主循环 ====================
int main()
{
    // 1. 创建协程
    Coroutine *co = new Coroutine(coroutine_func);

    // 2. 放入就绪队列
    ready_queue.push(co);

    std::cout << "[主线程] 启动事件循环...\n";

    // 3. 核心调度循环
    while (true)
    {
        // --- 阶段 A: 执行就绪任务 ---
        while (!ready_queue.empty())
        {
            Coroutine *c = ready_queue.front();
            ready_queue.pop();
            c->resume(); // 恢复协程执行
        }

        // --- 阶段 B: 计算休眠时间 ---
        // 计算距离下一个定时任务还有多久
        int timeout = timer_mgr.get_next_timeout();

        // --- 阶段 C: 陷入等待 ---
        // 如果没有任务了，退出循环
        if (timeout == -1 && ready_queue.empty())
        {
            std::cout << "[主线程] 所有任务完成，退出。\n";
            break;
        }

        // 调用你写好的 poll 函数
        // 注意：这里我们把 activeCoroutines 传进去，虽然本例不需要网络事件
        std::vector<Coroutine *> active_cos;
        epoller.poll(timeout, active_cos);

        // 这里不需要处理 active_cos，因为我们只是测试定时器

        // --- 阶段 D: 处理定时器 ---
        // 检查哪些定时器超时了，执行回调 (回调会把协程 push 回 ready_queue)
        timer_mgr.handle_expired_timers();
    }

    delete co;
    return 0;
}
