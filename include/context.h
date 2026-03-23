#pragma once

#include "utils.h"
#include "parameter.h"
#include <ucontext.h> // GNU ucontext API，用于用户态上下文管理

/**
 * 封装了 ucontext 上下文切换的一些操作，提供协程（用户态线程）功能
 */
namespace minico
{

    class Processor; // 前向声明，表示 Processor 类存在，但此处不需要完整定义

    class Context
    {
    public:
        /**
         * @brief 构造函数，分配栈空间并初始化上下文对象
         * @param stackSize 分配给此上下文的栈大小
         * @note 这里不会初始化具体入口函数，需通过 makeContext 设置
         */
        Context(size_t stackSize);

        /**
         * @brief 析构函数，释放分配的栈空间
         * @note 确保析构时不会释放空指针
         */
        ~Context();

        /**
         * @brief 拷贝构造函数
         * @note 仅拷贝 ctx_ 和 pStack_ 指针，实际上可能存在栈重复释放问题
         *       在实际项目中一般禁用拷贝构造，只使用移动构造
         */
        Context(const Context &otherCtx)
            : ctx_(otherCtx.ctx_), pStack_(otherCtx.pStack_)
        {
        }

        /**
         * @brief 移动构造函数
         * @note 将上下文资源（栈指针和上下文数据）从 otherCtx 移动到 this
         *       移动后建议将 otherCtx.pStack_ 置 nullptr 避免析构重复释放
         */
        Context(Context &&otherCtx)
            : ctx_(otherCtx.ctx_), pStack_(otherCtx.pStack_)
        {
        }

        /**
         * @brief 拷贝赋值操作符禁用
         * @note 不允许通过赋值拷贝 Context
         */
        Context &operator=(const Context &otherCtx) = delete;

        /**
         * @brief 设置上下文入口函数
         * @param func 要执行的函数
         * @param Processor* 指向调度器/父上下文
         * @param Context* 指向下一个上下文或传递给 func 的参数
         * @note 内部会调用 makecontext() 初始化 uc_stack、uc_link、uc_mcontext
         */
        void makeContext(void (*func)(), Processor *, Context *);

        /**
         * @brief 保存当前程序上下文
         * @note 内部会调用 getcontext(&ctx_)，把当前执行状态保存到 ctx_
         */
        void makeCurContext();

        /**
         * @brief 切换到当前上下文
         * @param pOldCtx 保存当前上下文的指针，如果为空则直接运行当前上下文
         * @note 内部调用 swapcontext 或 setcontext 实现原子切换
         */
        void swapToMe(Context *pOldCtx);

        /**
         * @brief 获取底层 ucontext_t 指针
         * @return ucontext_t* 指向 ctx_
         * @note 用于底层调用 swapcontext / setcontext
         */
        // 内联函数：建议编译器把函数代码直接展开，而不是进行函数调用
        // 允许函数在头文件中定义，避免链接错误
        inline ucontext_t *getUCtx() { return &ctx_; };

    private:
        ucontext_t ctx_; // GNU ucontext 上下文结构体，保存寄存器、栈指针、PC 等状态

        void *pStack_;     // 协程栈指针，由构造函数分配
        size_t stackSize_; // 协程栈大小，用于分配和释放
    };

}

/**
 * @section ucontext API 使用说明
 *
 * ucontext_t GNU 提供的一套用户态上下文管理 API，允许保存、恢复和切换上下文。
 *
 * typedef struct ucontext_t {
 *     struct ucontext_t* uc_link;  // 当前 context 执行结束后要执行的下一个 context
 *     sigset_t uc_sigmask;         // 执行当前 context 时需要屏蔽的信号列表
 *     stack_t uc_stack;            // 当前 context 使用的栈信息
 *     mcontext_t uc_mcontext;      // 保存具体程序状态（PC、SP、寄存器等），平台相关
 * };
 *
 * 核心函数：
 * - int getcontext(ucontext_t* ucp);
 *      保存当前上下文到 ucp
 * - void makecontext(ucontext_t* ucp, void (*func)(), int argc, ...);
 *      初始化 ucp 上下文，指定入口函数 func 和参数
 * - int swapcontext(ucontext_t* olducp, ucontext_t* newucp);
 *      原子保存当前上下文到 olducp 并切换到 newucp
 * - int setcontext(const ucontext_t* ucp);
 *      切换到 ucp 上下文，不返回
 *
 * 使用示例：
 * 1. 分配堆栈
 * 2. getcontext() 获取上下文模板
 * 3. 设置 uc_stack 和 uc_link
 * 4. 调用 makecontext() 指定入口函数
 * 5. swapcontext() 切换上下文
 *
 * 通过封装 Context 类，可以更方便地管理协程上下文和栈，不必直接操作 ucontext_t。
 */