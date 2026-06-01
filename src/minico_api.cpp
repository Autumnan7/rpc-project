#include "minico_api.h"
#include "logger.h"

void minico::co_go(std::function<void()> &&func, size_t stackSize, int tid)
{
	if (tid < 0)
	{
		// 1. 未指定目标线程（tid < 0），启用全局负载均衡机制
		// 例如客户端压测发起海量请求、或业务代码手动 throw 重计算任务
		// Scheduler 内部会调用 ProcessorSelector (如 MIN_EVENT_FIRST 算法) 选择最空闲的核心
		minico::Scheduler::getScheduler()->createNewCo(std::move(func), stackSize);
	}
	else
	{
		// 2. 指定了目标线程（tid >= 0），强制绕过负载均衡（就地执行）
		// 在 TcpServer::start_multi 时触发：内核基于 SO_REUSEPORT 算好 Hash 落到确定的 CPU 核
		// 为了保住 L1/L2 缓存，直接通过 tid 获取专属核的 Processor 进行极速处理
		tid %= minico::Scheduler::getScheduler()->getProCnt();
		minico::Scheduler::getScheduler()->getProcessor(tid)->goNewCo(std::move(func), stackSize);
	}
}

void minico::co_go(std::function<void()> &func, size_t stackSize, int tid)
{
	if (tid < 0)
	{
		LOG_INFO("CREATE A CO");
		minico::Scheduler::getScheduler()->createNewCo(func, stackSize);
	}
	else
	{
		tid %= minico::Scheduler::getScheduler()->getProCnt();
		minico::Scheduler::getScheduler()->getProcessor(tid)->goNewCo(func, stackSize);
	}
}

void minico::co_sleep(Time time)
{
	minico::Scheduler::getScheduler()->getProcessor(threadIdx)->wait(time);
}

void minico::sche_join()
{
	minico::Scheduler::getScheduler()->join();
}