#include "epoller.h"
#include "timer.h"
#include "mstime.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>

using namespace minico;

int main()
{
    Epoller epoller;
    Timer timer;

    if (!timer.init(&epoller))
    {
        std::cerr << "timer.init failed\n";
        return 1;
    }

    // 仅作为“标识符”使用，不会解引用
    Coroutine *co1 = reinterpret_cast<Coroutine *>(0x1);
    Coroutine *co2 = reinterpret_cast<Coroutine *>(0x2);

    const int64_t start = Time::now().getTimeVal();

    // 200ms / 500ms 两个任务，验证先后与延迟
    timer.runAfter(Time(200), co1);
    timer.runAfter(Time(500), co2);

    bool got1 = false;
    bool got2 = false;
    int64_t t1 = -1;
    int64_t t2 = -1;

    while (!(got1 && got2))
    {
        std::vector<Coroutine *> active;
        int n = epoller.poll(1000, active);
        if (n < 0)
        {
            std::cerr << "epoller.poll failed\n";
            return 2;
        }

        std::vector<Coroutine *> expired;
        timer.getExpiredCoroutines(expired);

        const int64_t now = Time::now().getTimeVal();
        for (Coroutine *co : expired)
        {
            if (co == co1 && !got1)
            {
                got1 = true;
                t1 = now - start;
                std::cout << "co1 fired at " << t1 << " ms\n";
            }
            else if (co == co2 && !got2)
            {
                got2 = true;
                t2 = now - start;
                std::cout << "co2 fired at " << t2 << " ms\n";
            }
        }
    }

    // 容忍调度抖动：co1 约 200ms，co2 约 500ms
    // 这里给一个宽松窗口，避免 CI/虚拟机误报
    if (!(t1 >= 120 && t1 <= 1200))
    {
        std::cerr << "co1 timing out of range: " << t1 << " ms\n";
        return 3;
    }

    if (!(t2 >= 350 && t2 <= 2000))
    {
        std::cerr << "co2 timing out of range: " << t2 << " ms\n";
        return 4;
    }

    if (!(t1 <= t2))
    {
        std::cerr << "order violation: t1=" << t1 << ", t2=" << t2 << "\n";
        return 5;
    }

    std::cout << "timer+epoller test passed\n";
    return 0;
}