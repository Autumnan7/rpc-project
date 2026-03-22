#include "context.h"
#include "parameter.h"
#include <stdlib.h>   // malloc / free
#include <ucontext.h> // getcontext, makecontext, swapcontext, setcontext

using namespace minico;

/**
 * @brief 构造函数
 * @param stackSize 分配给协程的栈大小
 * @note 这里只初始化成员变量，不分配栈，分配在 makeContext 中
 */
Context::Context(size_t stackSize) : pStack_(nullptr), stackSize_(stackSize) {}

/**
 * @brief 析构函数
 * @note 如果栈已经分配，则释放内存
 */
Context::~Context()
{
    if (pStack_)
    {
        free(pStack_);
    }
}

/**
 * @brief 初始化协程上下文
 * @param func 协程入口函数
 * @param pP 协程绑定的 Processor 对象（可用作参数传递）
 * @param pLink 下一个上下文指针，协程结束后切换到的上下文
 *
 * @note 流程：
 *  1. 如果栈尚未分配，调用 malloc 分配协程栈
 *  2. 调用 getcontext 获取当前上下文模板
 *  3. 设置 ctx_ 的栈信息和 uc_link
 *  4. 调用 makecontext 指定入口函数和参数
 */
void Context::makeContext(void (*func)(), Processor *pP, Context *pLink)
{
    // 如果栈未分配，则分配
    if (!pStack_)
    {
        pStack_ = malloc(stackSize_);
    }

    // 获取当前上下文状态，保存到 ctx_
    // :: 表示全局命名空间的 getcontext，避免与其他可能的同名函数冲突
    ::getcontext(&ctx_);

    // 设置协程栈指针和大小
    ctx_.uc_stack.ss_sp = pStack_;

    // TODO: 使用构造传入的 stackSize_，不要写死默认值
    ctx_.uc_stack.ss_size = parameter::coroutineStackSize;

    // 设置协程执行完后的下一个上下文
    ctx_.uc_link = pLink->getUCtx();

    // 设置协程入口函数和参数
    ::makecontext(&ctx_, func, 1, pP);
}

/**
 * @brief 保存当前程序上下文到 ctx_
 * @note 可用于后续切换回当前上下文
 */
void Context::makeCurContext()
{
    ::getcontext(&ctx_);
}

/**
 * @brief 切换到当前 Context
 * @param pOldCtx 保存当前上下文的 Context，如果为空则直接切换
 *
 * @note 流程：
 *  - 如果 pOldCtx 为 nullptr，直接 setcontext 切换，不会返回
 *  - 如果 pOldCtx 非空，swapcontext 保存原上下文到 pOldCtx 并切换到 ctx_
 */
void Context::swapToMe(Context *pOldCtx)
{
    if (nullptr == pOldCtx)
    {
        // 直接切换到当前上下文
        setcontext(&ctx_);
    }
    else
    {
        // 保存当前上下文到 pOldCtx->ctx_，切换到当前协程 ctx_
        swapcontext(pOldCtx->getUCtx(), &ctx_);
    }
}