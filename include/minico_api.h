#pragma once
#include "scheduler.h"
#include "mstime.h"

namespace minico
{

	/**
	 * @brief 运行一个协程
	 * @param func 运行的函数任务
	 * @param stackSize 运行的栈大小 默认为 parameter::coroutineStackSize
	 * @param tid Thread ID：选取的处理器编号 默认为-1 也就是使用策略调度器选择处理器
	 */
	void co_go(std::function<void()> &func, size_t stackSize = parameter::coroutineStackSize, int tid = -1);
	void co_go(std::function<void()> &&func, size_t stackSize = parameter::coroutineStackSize, int tid = -1);

	void co_sleep(Time t);

	/**
	 * @brief 阻塞等待所有工作线程结束
	 * @note - 调用链：sche_join() -> Scheduler::join() -> Processor::join() -> std::thread::join()
	 * @note - 这是一个阻塞函数，会一直阻塞直到所有 Processor 的工作线程结束
	 *       工作线程在调用 stop() 后才会结束其事件循环
	 */
	void sche_join();

}
