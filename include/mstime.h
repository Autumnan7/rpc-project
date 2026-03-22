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

        /**
         * @brief 将秒数转换为本地时间
         * @param second 以秒为单位的时间值
         * @param timezone 时区偏移量，单位为秒
         * @param tm_time 输出参数，指向一个 tm 结构体，用于存储转换后的本地时间
         */
        static void toLocalTime(time_t second, long timezone, struct tm *tm_time);

        struct timespec timeIntervalFromNow(); // 计算当前时间与对象时间的差值，返回一个 timespec 结构体

        int64_t getTimeVal() { return _timeVal; } // 负责读取已有对象的时间值

    private:
        int64_t _timeVal; // 以毫秒为单位的时间值
    };

    // lhs：left hand side
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
