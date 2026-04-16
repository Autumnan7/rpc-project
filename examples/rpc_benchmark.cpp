/**
 * @file rpc_benchmark.cpp
 * @brief RPC 性能基准测试 - 测试 QPS、延迟、并发性能
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>

#include "logger.h"
#include "rpc/rpc_client.h"
#include "minico_api.h"

/// 统计结果
struct BenchmarkResult
{
    int64_t total_requests;
    int64_t total_errors;
    double duration_ms;
    double qps;
    double avg_latency_us;
    double min_latency_us;
    double max_latency_us;
};

/// 线程安全的计数器
std::atomic<int64_t> g_total_requests{0};
std::atomic<int64_t> g_total_errors{0};
std::atomic<int64_t> g_total_latency_us{0};
std::atomic<int64_t> g_min_latency_us{INT64_MAX};
std::atomic<int64_t> g_max_latency_us{0};
std::atomic<bool> g_running{true};

/**
 * @brief 单客户端工作协程
 * @param client_id   客户端 ID
 * @param server_ip   服务器 IP
 * @param server_port 服务器端口
 * @param requests_per_client 每个客户端的请求数
 */
void benchmark_worker(int client_id, const char *server_ip, int server_port, int requests_per_client)
{
    RpcClient client;
    
    try
    {
        client.connect(server_ip, server_port);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("[Client %d] Connect failed: %s", client_id, e.what());
        return;
    }

    for (int i = 0; i < requests_per_client && g_running; ++i)
    {
        TinyJson request;
        TinyJson response;

        request["service"].Set<std::string>("SystemMonitorService");
        request["method"].Set<std::string>("GetStatus");

        auto start = std::chrono::high_resolution_clock::now();
        
        try
        {
            client.call(request, response);
            g_total_requests.fetch_add(1);
        }
        catch (const std::exception &e)
        {
            g_total_errors.fetch_add(1);
            continue;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // 更新延迟统计
        g_total_latency_us.fetch_add(latency_us);

        // 更新最小延迟
        int64_t current_min = g_min_latency_us.load();
        while (latency_us < current_min && !g_min_latency_us.compare_exchange_weak(current_min, latency_us));

        // 更新最大延迟
        int64_t current_max = g_max_latency_us.load();
        while (latency_us > current_max && !g_max_latency_us.compare_exchange_weak(current_max, latency_us));
    }
}

/**
 * @brief 运行基准测试
 * @param server_ip      服务器 IP
 * @param server_port    服务器端口
 * @param num_clients    并发客户端数
 * @param total_requests 总请求数
 */
BenchmarkResult run_benchmark(const char *server_ip, int server_port, int num_clients, int total_requests)
{
    // 重置计数器
    g_total_requests = 0;
    g_total_errors = 0;
    g_total_latency_us = 0;
    g_min_latency_us = INT64_MAX;
    g_max_latency_us = 0;
    g_running = true;

    int requests_per_client = total_requests / num_clients;

    std::cout << "========================================" << std::endl;
    std::cout << "RPC Benchmark Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Target: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Clients: " << num_clients << std::endl;
    std::cout << "Total Requests: " << total_requests << std::endl;
    std::cout << "Requests/Client: " << requests_per_client << std::endl;
    std::cout << "========================================" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // 创建客户端协程
    for (int i = 0; i < num_clients; ++i)
    {
        minico::co_go([i, server_ip, server_port, requests_per_client]()
                      { benchmark_worker(i, server_ip, server_port, requests_per_client); });
    }

    // 等待所有请求完成
    while (g_total_requests.load() + g_total_errors.load() < total_requests && g_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end = std::chrono::high_resolution_clock::now();

    // 计算结果
    BenchmarkResult result;
    result.total_requests = g_total_requests.load();
    result.total_errors = g_total_errors.load();
    result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.qps = result.total_requests * 1000.0 / result.duration_ms;
    result.avg_latency_us = result.total_requests > 0 
        ? static_cast<double>(g_total_latency_us.load()) / result.total_requests 
        : 0;
    result.min_latency_us = g_min_latency_us.load() == INT64_MAX ? 0 : g_min_latency_us.load();
    result.max_latency_us = g_max_latency_us.load();

    return result;
}

void print_result(const BenchmarkResult &result)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Benchmark Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total Requests:  " << result.total_requests << std::endl;
    std::cout << "Total Errors:    " << result.total_errors << std::endl;
    std::cout << "Duration:        " << result.duration_ms << " ms" << std::endl;
    std::cout << "QPS:             " << static_cast<int64_t>(result.qps) << std::endl;
    std::cout << "Avg Latency:     " << result.avg_latency_us << " us" << std::endl;
    std::cout << "Min Latency:     " << result.min_latency_us << " us" << std::endl;
    std::cout << "Max Latency:     " << result.max_latency_us << " us" << std::endl;
    std::cout << "========================================" << std::endl;
}

int main(int argc, char *argv[])
{
    const char *server_ip = "127.0.0.1";
    int server_port = 12345;
    int num_clients = 10;       // 并发客户端数
    int total_requests = 10000; // 总请求数

    // 解析命令行参数
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc)
        {
            num_clients = std::stoi(argv[++i]);
        }
        else if (arg == "-n" && i + 1 < argc)
        {
            total_requests = std::stoi(argv[++i]);
        }
        else if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -c <clients>    Number of concurrent clients (default: 10)" << std::endl;
            std::cout << "  -n <requests>   Total number of requests (default: 10000)" << std::endl;
            std::cout << "  -h, --help      Show this help message" << std::endl;
            return 0;
        }
    }

    auto result = run_benchmark(server_ip, server_port, num_clients, total_requests);
    print_result(result);

    return 0;
}
