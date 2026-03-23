#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "parameter.h"
#include "processor.h"

int main()
{
    using namespace minico;
    using namespace std::chrono_literals;

    Processor processor(0);
    if (!processor.loop())
    {
        std::cerr << "[FAIL] processor.loop() init failed" << std::endl;
        return 1;
    }

    std::atomic<int> done{0};
    std::atomic<bool> waitResumed{false};
    const auto t0 = std::chrono::steady_clock::now();

    processor.goNewCo([&done]()
                      { done.fetch_add(1, std::memory_order_relaxed); }, parameter::coroutineStackSize);

    processor.goNewCo([&done]()
                      { done.fetch_add(1, std::memory_order_relaxed); }, parameter::coroutineStackSize);

    processor.goNewCo([&processor, &done, &waitResumed, t0]()
                      {
        processor.wait(Time(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0);
        if (elapsed.count() >= 20)
        {
            waitResumed.store(true, std::memory_order_relaxed);
        }
        done.fetch_add(1, std::memory_order_relaxed); }, parameter::coroutineStackSize);

    const auto start = std::chrono::steady_clock::now();
    bool timeout = false;
    while (done.load(std::memory_order_relaxed) < 3)
    {
        if (std::chrono::steady_clock::now() - start > 2s)
        {
            timeout = true;
            break;
        }
        std::this_thread::sleep_for(5ms);
    }

    processor.stop();
    processor.join();

    if (timeout)
    {
        std::cerr << "[FAIL] timeout waiting test coroutines" << std::endl;
        return 2;
    }

    if (done.load(std::memory_order_relaxed) != 3)
    {
        std::cerr << "[FAIL] done count mismatch: " << done.load() << std::endl;
        return 3;
    }

    if (!waitResumed.load(std::memory_order_relaxed))
    {
        std::cerr << "[FAIL] wait coroutine did not resume as expected" << std::endl;
        return 4;
    }

    std::cout << "[PASS] processor scheduling + wait resume works" << std::endl;
    return 0;
}
