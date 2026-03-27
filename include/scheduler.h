#pragma once
#include <vector>
#include <functional>

#include "processor.h"
#include "processor_selector.h"

/**
 * @brief 协程调度器：
 *        允许用户指定协程运行在某个 Processor 上；
 *        若未指定，则自动选择协程数量最少的 Processor 接管新协程。
 */
namespace minico
{
	class Scheduler
	{
	protected:
		Scheduler();
		~Scheduler();

	public:
		DISALLOW_COPY_MOVE_AND_ASSIGN(Scheduler);

		/** 获取全局唯一的协程调度器实例 */
		static Scheduler *getScheduler();

		/** 创建新协程 */
		void createNewCo(std::function<void()> &&func, size_t stackSize);
		void createNewCo(std::function<void()> &func, size_t stackSize);

		/** 获取指定编号的 Processor */
		Processor *getProcessor(int);

		/** 获取当前管理的 Processor 数量 */
		int getProCnt();

		/** 停止调度器运行 */
		void join();

	private:
		/** 创建 threadCnt 个 Processor 并启动事件循环 */
		bool startScheduler(int threadCnt);

		/** 全局唯一的协程调度器实例 */
		static Scheduler *pScher_;

		/** 保护单例初始化的互斥锁 */
		static std::mutex scherMtx_;

		/** 调度器管理的 Processor 列表 */
		std::vector<Processor *> processors_;

		/** 协程分发器 */
		ProcessorSelector proSelector_;
	};
}