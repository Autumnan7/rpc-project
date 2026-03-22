#include "timer.h"
#include "epoller.h"
#include "coroutine.h"

#include <cstdint>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

using namespace minico;

Timer::Timer() : timeFd_(-1)
{
}

Timer::~Timer()
{
    if (isTimeFdUseful())
    {
        close(timeFd_);
    }
}

bool Timer::init(Epoller *pEpoller)
{
    timeFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (isTimeFdUseful())
    {
        // 监听timerfd
        return pEpoller->addEvent(nullptr, timeFd_, EPOLLIN | EPOLLPRI | EPOLLRDHUP);
    }
    return false;
}

// TODO: 验证是否正确
void Timer::getExpiredCoroutines(std::vector<Coroutine *> &expiredCoroutines)
{
    // 1. 【必须第一步】消费掉 timerfd 上的数据
    // 无论是否有任务超时，都要清空内核的计数器，否则 epoll 会一直触发 EPOLLIN
    uint64_t buf;
    while (::read(timeFd_, &buf, sizeof(buf)) > 0)
    {
        // 非阻塞模式，读空即退出
    }

    // 2. 获取当前时间，检查超时任务
    Time nowTime = Time::now();
    while (!timerHeap_.empty())
    {
        // 获取堆顶（最早时间）
        const auto &top = timerHeap_.top();

        if (top.first <= nowTime)
        {
            // 超时，加入结果集
            expiredCoroutines.push_back(top.second);
            timerHeap_.pop();
        }
        else
        {
            // 堆顶都没超时，后面的更不用看了
            break;
        }
    }

    // 3. 重置定时器
    // 如果堆不为空，说明还有后续任务，重新设置 timerfd 的唤醒时间
    if (!timerHeap_.empty())
    {
        setTimerfdExpiration(timerHeap_.top().first);
    }
}

void Timer::runAt(Time time, Coroutine *pCo)
{
    bool earliestChanged = timerHeap_.empty() || time < timerHeap_.top().first;

    timerHeap_.emplace(time, pCo);

    if (earliestChanged)
    {
        setTimerfdExpiration(time);
    }
}

void Timer::runAfter(Time delay, Coroutine *pCo)
{
    if (pCo == nullptr)
    {
        return;
    }

    const auto nowMs = Time::now().getTimeVal();
    const auto delayMs = delay.getTimeVal();
    const auto runAtMs = nowMs + (delayMs > 0 ? delayMs : 0);

    runAt(Time(runAtMs), pCo);
}

bool Timer::setTimerfdExpiration(Time time)
{
    if (!isTimeFdUseful())
    {
        return false;
    }

    itimerspec spec{};                          // 零初始化
    spec.it_value = time.timeIntervalFromNow(); // 相对当前时间的触发间隔

    // old_value 不需要时可传 nullptr
    return ::timerfd_settime(timeFd_, 0, &spec, nullptr) == 0;
}

void Timer::wakeUp()
{
    setTimerfdExpiration(Time::now());
}