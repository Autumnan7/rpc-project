#pragma once
#include <stdint.h>
#include <time.h>

struct timespec; //

namespace minico
{

    const char days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    class Time
    {
    public:
        Time(int64_t msSinceEpoch) : _timeVal(msSinceEpoch) {}

        Time(const Time &time) { _timeVal = time._timeVal; }

        Time(const Time &&time) { _timeVal = time._timeVal; }

        Time &operator=(const Time &time)
        {
            _timeVal = time._timeVal;
            return *this;
        }

        ~Time() {}

        static Time now(); // 负责生成当前时间,单位毫秒

        static time_t nowSec(); // 获取当前时间，单位秒

        static void toLocalTime(time_t second, long timezone, struct tm *tm_time);

        struct timespec timeIntervalFromNow();

        int64_t getTimeVal() { return _timeVal; } // 负责读取已有对象的时间值

    private:
        int64_t _timeVal; // 以毫秒为单位的时间值
    };

    // left hand sied
    inline bool operator<(Time lhs, Time rhs)
    {
        return lhs.getTimeVal() < rhs.getTimeVal();
    }

    inline bool operator<=(Time lhs, Time rhs)
    {
        return lhs.getTimeVal() <= rhs.getTimeVal();
    }

    inline bool operator>(Time lhs, Time rhs)
    {
        return lhs.getTimeVal() > rhs.getTimeVal();
    }

    inline bool operator>=(Time lhs, Time rhs)
    {
        return lhs.getTimeVal() >= rhs.getTimeVal();
    }

    inline bool operator==(Time lhs, Time rhs)
    {
        return lhs.getTimeVal() == rhs.getTimeVal();
    }

}
