#pragma once
#include "mstime.h"
#include "utils.h"

#include <queue>
#include <vector>
/**
 * timefd配合一个小根堆来实现
 * 小根堆存放的是时间和协程对象的pair
 */
#define TIMER_DUMMYBUF_SIZE 1024

namespace minico
{
    class Coroutine;

    class Epoller;

    class Timer
    {
    public:
        using TimerHeap = typename std::priority_queue<
            std::pair<Time, Coroutine *>,
            std::vector<std::pair<Time, Coroutine *>>,
            std::greater<std::pair<Time, Coroutine *>>>;

        /**
         * @brief 初始化定时器
         * @param epoller 定时器需要与 epoller 进行交互，因此需要
         * @return 返回一个布尔值，表示初始化是否成功
         */
        bool init(Epoller *);

        Timer();
        ~Timer();

        DISALLOW_COPY_MOVE_AND_ASSIGN(Timer);

        /** 返回超时协程任务队列*/
        void getExpiredCoroutines(std::vector<Coroutine *> &expiredCoroutines);

        /** 在time时刻运行协程*/
        void runAt(Time time, Coroutine *pCo);

        /** 经过delay时间后运行协程*/
        void runAfter(Time delay, Coroutine *pCo);

        /** 唤醒epoll控制器*/
        void wakeUp();

    private:
        bool setTimerfdExpiration(Time time);

        inline bool isTimeFdUseful() const { return timeFd_ >= 0; }

        int timeFd_;

        // 接收 timerfd 触发时读取的“哑”数据，防止 fd 一直可读
        char dummyBuf_[TIMER_DUMMYBUF_SIZE];

        // 存储定时任务的堆结构
        TimerHeap timerHeap_;
    };

}