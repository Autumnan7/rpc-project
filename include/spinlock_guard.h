// RAII（Resource Acquisition Is Initialization）是 C++ 的核心思想
// 资源获取即初始化，资源释放即析构。

#pragma once
#include "spinlock.h"
#include "utils.h"

namespace minico {

	class SpinlockGuard
	{
	public:
		
		SpinlockGuard(Spinlock& l)
			: lock_(l)
		{
			lock_.lock();
		}

		~SpinlockGuard()
		{
			lock_.unlock();
		}

		DISALLOW_COPY_MOVE_AND_ASSIGN(SpinlockGuard);

	private:
		Spinlock& lock_;

	};

}
