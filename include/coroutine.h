#pragma once
#include <functional>
#include "context.h"
#include "utils.h"
#include "logger.h"

namespace minico
{

    // 协程状态
    enum coStatus
    {
        CO_READY = 0,
        CO_RUNNING,
        CO_SUSPEND,
        CO_DEAD
    };

    class Processor;

    class Coroutine
    {
    public:
        // std::function<void()> func; 表示 func 是一个装有“无参无返回值函数”的盒子。
        // 声明中可以省略 变量名称
        Coroutine(Processor *, size_t stackSize, std::function<void()> &&);
        // const 表示不会更改
        Coroutine(Processor *, size_t stackSize, const std::function<void()> &);

        ~Coroutine();

        DISALLOW_COPY_MOVE_AND_ASSIGN(Coroutine);

        // 恢复协程的执行
        void resume();

        // 切出当前协程
        void yield();

        // 执行协程函数体
        inline void startFunc()
        {
            if (func_)
            {
                func_();
            }
        }

        // 获取协程上下文
        inline Context *getCtx() { return &ctx_; }

        coStatus getStatus() const { return status_; }

    private:
        std::function<void()> func_; // 协程要执行的函数

        Processor *processor_; // 协程所属的调度器

        coStatus status_; // 协程的状态

        Context ctx_; // 协程的上下文
    };
}