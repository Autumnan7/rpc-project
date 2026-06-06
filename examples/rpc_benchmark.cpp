/**
 * @file rpc_benchmark.cpp
 * @brief RPC 性能基准测试工具 (Time-based 高并发压测版)
 *
 * 核心特性：
 *   - 采用全异步协程+连接池(长连接)架构，支持单机 1w+ 极高并发打流。
 *   - 时间驱动型的打流(无 sleep 满载压榨)，完美呈现系统在 C10K 下的 QPS 极限。
 *   - 基于线程本地缓存与无锁原子(atomic)统计，彻底消除统计模块本身的锁竞争。
 *   - 自动生成 P50/P90/P95/P99/P99.9 延迟分布与吞吐量报告。
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"
#include "minico_api.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_header.h"

namespace
{

struct BenchConfig
{
    int clients = 10000;       // C10K 默认配置
    int duration_sec = 10;     // 10 秒满载压测
    std::string ip = "127.0.0.1";
    int port = 12345;
    std::string service = "SystemMonitorService";
    std::string method = "GetStatus";
};

// 无锁并发统计器
std::atomic<int64_t> g_success{0};
std::atomic<int64_t> g_failed{0};
std::atomic<int64_t> g_bytes{0};

// 延迟收集
std::mutex g_latency_mutex;
std::vector<uint32_t> g_latencies;
std::atomic<int> g_active_workers{0};

void print_help(const char *bin)
{
    std::cout << "Usage: " << bin << " [options] [target]\n"
              << "Options:\n"
              << "  -c <clients>   并发协程/连接数 (default: 10000)\n"
              << "  -t <seconds>   压测持续时间 (default: 10)\n"
              << "  -p <port>      服务端端口 (default: 12345)\n"
              << "  -s <service>   RPC 服务名 (default: SystemMonitorService)\n"
              << "  -m <method>    RPC 方法名 (default: GetStatus)\n"
              << "  -h, --help     显示帮助信息\n"
              << "\n"
              << "Target examples:\n"
              << "  127.0.0.1:12345\n";
}

bool parse_target(const std::string &target, std::string &ip, int &port)
{
    if (target.empty()) return false;

    const size_t colon_pos = target.rfind(':');
    if (colon_pos == std::string::npos)
    {
        ip = target;
        return true;
    }

    const std::string host = target.substr(0, colon_pos);
    const std::string port_str = target.substr(colon_pos + 1);
    if (host.empty() || port_str.empty()) return false;

    try
    {
        port = std::stoi(port_str);
        ip = host;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void rpc_worker(BenchConfig cfg,
                std::chrono::steady_clock::time_point deadline,
                int64_t req_bytes)
{
    g_active_workers.fetch_add(1, std::memory_order_relaxed);
    RpcClient client;
    bool connected = false;

    TinyJson request;
    request["service"].Set<std::string>(cfg.service);
    request["method"].Set<std::string>(cfg.method);

    // 每个协程自己维护延迟数据，避免全量锁竞争
    std::vector<uint32_t> local_latencies;
    local_latencies.reserve(20000); 

    while (std::chrono::steady_clock::now() < deadline)
    {
        try
        {
            if (!connected)
            {
                client.connect(cfg.ip.c_str(), cfg.port);
                connected = true;
            }

            TinyJson response;
            auto start_time = std::chrono::steady_clock::now();
            client.call(request, response);
            auto end_time = std::chrono::steady_clock::now();
            
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            local_latencies.push_back(static_cast<uint32_t>(latency_us));

            const std::string response_body = response.WriteJson();

            g_success.fetch_add(1, std::memory_order_relaxed);
            g_bytes.fetch_add(
                req_bytes + static_cast<int64_t>(sizeof(RpcHeader)) + static_cast<int64_t>(response_body.size()),
                std::memory_order_relaxed);
        }
        catch (...)
        {
            g_failed.fetch_add(1, std::memory_order_relaxed);
            if (connected)
            {
                client.close();
                connected = false;
            }
        }
    }

    if (connected)
    {
        client.close();
    }

    // 协程退出时，统一合并延迟数据
    {
        std::lock_guard<std::mutex> lock(g_latency_mutex);
        g_latencies.insert(g_latencies.end(), local_latencies.begin(), local_latencies.end());
    }
    g_active_workers.fetch_sub(1, std::memory_order_relaxed);
}

} // namespace

int main(int argc, char *argv[])
{
    BenchConfig cfg;
    std::string target;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help"))
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-c" && i + 1 < argc)
        {
            cfg.clients = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "-t" && i + 1 < argc)
        {
            cfg.duration_sec = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "-p" && i + 1 < argc)
        {
            cfg.port = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "-s" && i + 1 < argc)
        {
            cfg.service = argv[++i];
            continue;
        }
        if (arg == "-m" && i + 1 < argc)
        {
            cfg.method = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] != '-')
        {
            target = arg;
            continue;
        }

        std::cerr << "Invalid argument: " << arg << std::endl;
        return 1;
    }

    if (!target.empty() && !parse_target(target, cfg.ip, cfg.port))
    {
        std::cerr << "Invalid target format." << std::endl;
        return 1;
    }

    // 预计算请求包大小
    TinyJson req_preview;
    req_preview["service"].Set<std::string>(cfg.service);
    req_preview["method"].Set<std::string>(cfg.method);
    const int64_t req_bytes = static_cast<int64_t>(sizeof(RpcHeader)) +
                              static_cast<int64_t>(req_preview.WriteJson().size());

    // 关闭普通日志以应对极高并发压测（消除 I/O 干扰）
    setLogLevel(LOG_LEVEL_FATAL);

    std::cout << "========================================\n"
              << "   Rpcbench - Coroutine RPC Benchmark   \n"
              << "========================================\n"
              << "Service : " << cfg.service << "." << cfg.method << "\n"
              << "Target  : " << cfg.ip << ":" << cfg.port << "\n"
              << "Clients : " << cfg.clients << " (coroutines)\n"
              << "Time    : " << cfg.duration_sec << " seconds\n"
              << "----------------------------------------\n" << std::endl;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(cfg.duration_sec);
    
    // 派发压测协程
    for (int i = 0; i < cfg.clients; ++i)
    {
        minico::co_go([cfg, deadline, req_bytes]()
                      { rpc_worker(cfg, deadline, req_bytes); });
    }

    // 主线程休眠等待压测结束
    std::this_thread::sleep_until(deadline);

    // 等待所有协程工作完全终止并合并统计数据
    auto wait_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (g_active_workers.load(std::memory_order_relaxed) > 0 && std::chrono::steady_clock::now() < wait_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // =================结果计算=================
    const int64_t success = g_success.load(std::memory_order_relaxed);
    const int64_t failed = g_failed.load(std::memory_order_relaxed);
    const int64_t bytes = g_bytes.load(std::memory_order_relaxed);

    const int64_t qps = static_cast<int64_t>(success * 1.0 / cfg.duration_sec);
    const int64_t bytes_per_sec = static_cast<int64_t>(bytes * 1.0 / cfg.duration_sec);
    const double mb_per_sec = bytes_per_sec / (1024.0 * 1024.0);

    std::cout << "[ Benchmark Results ]\n";
    std::cout << "Total Requests : " << (success + failed) << "\n"
              << "Succeed        : " << success << "\n"
              << "Failed         : " << failed << "\n"
              << "Throughput     : " << qps << " QPS (Req/sec)\n"
              << "Network I/O    : " << std::fixed << std::setprecision(2) << mb_per_sec << " MB/sec\n";

    {
        std::lock_guard<std::mutex> lock(g_latency_mutex);
        if (!g_latencies.empty())
        {
            std::sort(g_latencies.begin(), g_latencies.end());
            const size_t n = g_latencies.size();
            const double min_lat = g_latencies.front() / 1000.0;
            const double max_lat = g_latencies.back() / 1000.0;
            const double p50 = g_latencies[static_cast<size_t>(n * 0.50)] / 1000.0;
            const double p90 = g_latencies[static_cast<size_t>(n * 0.90)] / 1000.0;
            const double p95 = g_latencies[static_cast<size_t>(n * 0.95)] / 1000.0;
            const double p99 = g_latencies[static_cast<size_t>(n * 0.99)] / 1000.0;
            const double p999 = g_latencies[static_cast<size_t>(n * 0.999)] / 1000.0;

            std::cout << "\n[ Latency Distribution (ms) ]\n"
                      << std::fixed << std::setprecision(2)
                      << "  Min   : " << min_lat << " ms\n"
                      << "  P50   : " << p50 << " ms\n"
                      << "  P90   : " << p90 << " ms\n"
                      << "  P95   : " << p95 << " ms\n"
                      << "  P99   : " << p99 << " ms\n"
                      << "  P99.9 : " << p999 << " ms\n"
                      << "  Max   : " << max_lat << " ms\n";
        }
    }
    
    // minico::sche_join(); // 如果需要彻底退出系统可在此处添加
    return 0;
}
