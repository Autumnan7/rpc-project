#include <mstime.h>
#include <sys/time.h>

using namespace minico;

Time Time::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t ms = static_cast<int64_t>(tv.tv_sec) * 1000 + static_cast<int64_t>(tv.tv_usec) / 1000;
    return Time(ms);
}

time_t Time::nowSec()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec;
}

void Time::toLocalTime(time_t second, long timezone, struct tm *tm_time)
{
    time_t local_time = second - timezone;
    localtime_r(&local_time, tm_time);
}

struct timespec Time::timeIntervalFromNow()
{
    struct timespec ts;
    int64_t microseconds = _timeVal - Time::now().getTimeVal();
    if (microseconds < 1)
    {
        ts.tv_sec = 0;
        ts.tv_nsec = 1000;
    }
    else
    {
        ts.tv_sec = static_cast<time_t>(
            microseconds / 1000);
        ts.tv_nsec = static_cast<long>(
            (microseconds % 1000) * 1000 * 1000);
    }
    return ts;
}