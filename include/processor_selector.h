#pragma once
#include <vector>

namespace minico
{
	class Processor;

	// 协程调度策略
	enum scheduleStrategy
	{
		MIN_EVENT_FIRST = 0, // 优先选择当前事件最少的 Processor
		ROUND_ROBIN			 // 轮询分配
	};

	/** 协程调度策略管理器：负责根据策略选择下一个接收协程的 Processor */
	class ProcessorSelector
	{
	public:
		ProcessorSelector(std::vector<Processor *> &processors, int strategy = MIN_EVENT_FIRST)
			: curPro_(-1), strategy_(strategy), processors_(processors) {}

		~ProcessorSelector() {}

		/** 设置调度策略 */
		inline void setStrategy(int strategy) { strategy_ = strategy; }

		/** 获取下一个被调度的 Processor */
		Processor *next();

	private:
		/** 当前 Processor 下标 */
		int curPro_;

		/** 当前调度策略 */
		int strategy_;

		/** 外部维护的 Processor 列表引用 */
		std::vector<Processor *> &processors_;
	};
}