/**
 * @file rpc_benchmark.cpp
 * @brief RPC 性能基准测试 - 协程并发 vs 同步串行 vs 多线程协程
 *
 * 测试场景（公平对比）：
 *   Test 1: 1 线程 + N 协程并发（体现协程事件驱动的并发能力）
 *   Test 2: 1 线程串行同步请求（传统阻塞 IO 基准线）
 *   Test 3: M 线程 + N 协程并发（体现多线程 + 协程的扩展能力）
 *
 * 指标收集：
 *   - QPS (吞吐量)
 *   - 平均/最小/最大延迟
 *   - P50/P90/P99 百分位延迟
 *
 * 使用方式：
 *   先启动 rpc_server，再运行本程序
 *   ./bin/rpc_benchmark [-n requests] [-p port] [--sweep]
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "rpc/rpc_client.h"
#include "rpc/rpc_header.h"
#include "minico_api.h"
#include "json.h"
#include "logger.h"

// ============================================================================
// 同步阻塞版 RPC 客户端（用于 Test 2 对比）
// ============================================================================

class SyncRpcClient
{
public:
    SyncRpcClient() : sockfd_(-1) {}
    ~SyncRpcClient() { close(); }

    void connect(const char *ip, int port)
    {
        sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0)
            throw std::runtime_error("socket() failed");

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        if (::connect(sockfd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            ::close(sockfd_);
            sockfd_ = -1;
            throw std::runtime_error("connect() failed");
        }

        int flag = 1;
        setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    }

    void call(TinyJson &request, TinyJson &response)
    {
        std::string json_str = request.WriteJson();
        uint32_t len = json_str.size();

        RpcHeader header;
        header.info = 0;
        header.magic = 0x7777;
        header.len = htonl(len);

        // 发送 header + body
        std::vector<char> buf(sizeof(header) + len);
        std::memcpy(buf.data(), &header, sizeof(header));
        std::memcpy(buf.data() + sizeof(header), json_str.data(), len);
        send_all(buf.data(), buf.size());

        // 接收 header
        RpcHeader resp_header;
        recv_all(&resp_header, sizeof(resp_header));

        // 接收 body
        uint32_t resp_len = ntohl(resp_header.len);
        std::vector<char> resp_buf(resp_len + 1);
        recv_all(resp_buf.data(), resp_len);
        resp_buf[resp_len] = '\0';
        response.ReadJson(std::string(resp_buf.data(), resp_len));
    }

    void close()
    {
        if (sockfd_ >= 0)
        {
            ::close(sockfd_);
            sockfd_ = -1;
        }
    }

private:
    void send_all(const void *data, size_t len)
    {
        size_t sent = 0;
        while (sent < len)
        {
            ssize_t n = ::send(sockfd_, (const char *)data + sent, len - sent, 0);
            if (n <= 0)
                throw std::runtime_error("send failed");
            sent += n;
        }
    }

    void recv_all(void *data, size_t len)
    {
        size_t received = 0;
        while (received < len)
        {
            ssize_t n = ::recv(sockfd_, (char *)data + received, len - received, 0);
            if (n <= 0)
                throw std::runtime_error("recv failed");
            received += n;
        }
    }

    int sockfd_;
};

// ============================================================================
// 统计工具 - 支持百分位延迟计算
// ============================================================================

struct BenchmarkResult
{
    std::string mode_name;
    int num_concurrent;
    int64_t total_requests;
    int64_t total_errors;
    double duration_ms;
    double qps;
    double avg_latency_us;
    double min_latency_us;
    double max_latency_us;
    double p50_latency_us;
    double p90_latency_us;
    double p99_latency_us;
};

/**
 * @brief 线程安全的延迟收集器
 * 
 * 每个协程/线程将延迟数据写入本地 vector，
 * 最后合并排序，计算百分位延迟。
 */
class LatencyCollector
{
public:
    void record(int64_t latency_us)
    {
        std::lock_guard<std::mutex> lock(mu_);
        latencies_.push_back(latency_us);
    }

    void record_error()
    {
        errors_.fetch_add(1, std::memory_order_relaxed);
    }

    int64_t done() const
    {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int64_t>(latencies_.size()) + errors_.load(std::memory_order_relaxed);
    }

    int64_t success_count() const
    {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int64_t>(latencies_.size());
    }

    BenchmarkResult compute(const std::string &name, int concurrency, double duration_ms) const
    {
        BenchmarkResult r{};
        r.mode_name = name;
        r.num_concurrent = concurrency;
        r.total_errors = errors_.load(std::memory_order_relaxed);
        r.duration_ms = duration_ms;

        std::vector<int64_t> sorted;
        {
            std::lock_guard<std::mutex> lock(mu_);
            sorted = latencies_;
        }

        r.total_requests = static_cast<int64_t>(sorted.size());
        if (sorted.empty())
        {
            r.qps = 0;
            return r;
        }

        std::sort(sorted.begin(), sorted.end());

        r.qps = r.total_requests * 1000.0 / duration_ms;

        int64_t sum = 0;
        for (auto v : sorted)
            sum += v;
        r.avg_latency_us = static_cast<double>(sum) / sorted.size();

        r.min_latency_us = static_cast<double>(sorted.front());
        r.max_latency_us = static_cast<double>(sorted.back());

        auto percentile = [&](double p) -> double
        {
            size_t idx = static_cast<size_t>(p / 100.0 * (sorted.size() - 1));
            return static_cast<double>(sorted[idx]);
        };

        r.p50_latency_us = percentile(50);
        r.p90_latency_us = percentile(90);
        r.p99_latency_us = percentile(99);

        return r;
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mu_);
        latencies_.clear();
        errors_.store(0, std::memory_order_relaxed);
    }

private:
    mutable std::mutex mu_;
    std::vector<int64_t> latencies_;
    std::atomic<int64_t> errors_{0};
};

static LatencyCollector g_collector;

// ============================================================================
// Test 1: 单线程 N 协程并发
// ============================================================================

static void coroutine_worker(const char *ip, int port, int count)
{
    RpcClient client;
    try
    {
        client.connect(ip, port);
    }
    catch (...)
    {
        for (int i = 0; i < count; ++i)
            g_collector.record_error();
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        TinyJson req, resp;
        req["service"].Set<std::string>("SystemMonitorService");
        req["method"].Set<std::string>("GetStatus");

        auto t0 = std::chrono::high_resolution_clock::now();
        try
        {
            client.call(req, resp);
        }
        catch (...)
        {
            g_collector.record_error();
            continue;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        g_collector.record(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
    }
}

static BenchmarkResult run_coroutine_test(const char *ip, int port,
                                          int num_co, int total_req,
                                          int tid_pin)
{
    g_collector.reset();
    int per_co = total_req / num_co;
    int remainder = total_req % num_co;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_co; ++i)
    {
        int count = per_co + (i < remainder ? 1 : 0);
        minico::co_go(
            [ip, port, count]()
            { coroutine_worker(ip, port, count); },
            minico::parameter::coroutineStackSize,
            tid_pin);
    }

    // 等待所有请求完成
    while (g_collector.done() < total_req)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::string name;
    if (tid_pin >= 0)
    {
        name = "Coroutine (1T x " + std::to_string(num_co) + "co)";
    }
    else
    {
        name = "Coroutine (MT x " + std::to_string(num_co) + "co)";
    }
    return g_collector.compute(name, num_co, duration_ms);
}

// ============================================================================
// Test 2: 单线程串行同步请求
// ============================================================================

static BenchmarkResult run_sync_test(const char *ip, int port, int total_req)
{
    g_collector.reset();

    SyncRpcClient client;
    try
    {
        client.connect(ip, port);
    }
    catch (const std::exception &e)
    {
        std::cerr << "  Sync connect failed: " << e.what() << std::endl;
        BenchmarkResult r{};
        r.mode_name = "Sync (serial)";
        r.num_concurrent = 1;
        r.total_errors = total_req;
        return r;
    }

    TinyJson req;
    req["service"].Set<std::string>("SystemMonitorService");
    req["method"].Set<std::string>("GetStatus");

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_req; ++i)
    {
        TinyJson resp;
        auto t0 = std::chrono::high_resolution_clock::now();
        try
        {
            client.call(req, resp);
        }
        catch (...)
        {
            g_collector.record_error();
            continue;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        g_collector.record(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return g_collector.compute("Sync (serial)", 1, duration_ms);
}

// ============================================================================
// 结果输出
// ============================================================================

static void print_single_result(const BenchmarkResult &r)
{
    std::cout << "  Requests OK : " << r.total_requests << std::endl;
    std::cout << "  Errors      : " << r.total_errors << std::endl;
    std::cout << "  Duration    : " << std::fixed << std::setprecision(1)
              << r.duration_ms << " ms" << std::endl;
    std::cout << "  QPS         : " << static_cast<int64_t>(r.qps) << std::endl;
    std::cout << "  Latency(us) : avg=" << std::fixed << std::setprecision(1)
              << r.avg_latency_us
              << "  min=" << r.min_latency_us
              << "  max=" << r.max_latency_us << std::endl;
    std::cout << "  Percentile  : P50=" << r.p50_latency_us
              << "  P90=" << r.p90_latency_us
              << "  P99=" << r.p99_latency_us << std::endl;
}

static void print_summary_table(const std::vector<BenchmarkResult> &results)
{
    std::cout << "\n" << std::string(110, '=') << std::endl;
    std::cout << "  Performance Summary" << std::endl;
    std::cout << std::string(110, '=') << std::endl;

    // 表头
    std::cout << std::left
              << std::setw(28) << "  Mode"
              << std::right
              << std::setw(6) << "Conc"
              << std::setw(10) << "QPS"
              << std::setw(10) << "Avg(us)"
              << std::setw(10) << "P50(us)"
              << std::setw(10) << "P90(us)"
              << std::setw(10) << "P99(us)"
              << std::setw(10) << "Min(us)"
              << std::setw(10) << "Max(us)"
              << std::setw(10) << "Dur(ms)"
              << std::endl;

    std::cout << std::string(110, '-') << std::endl;

    for (const auto &r : results)
    {
        std::cout << std::left
                  << std::setw(28) << ("  " + r.mode_name)
                  << std::right
                  << std::setw(6) << r.num_concurrent
                  << std::setw(10) << static_cast<int64_t>(r.qps)
                  << std::setw(10) << std::fixed << std::setprecision(0) << r.avg_latency_us
                  << std::setw(10) << std::fixed << std::setprecision(0) << r.p50_latency_us
                  << std::setw(10) << std::fixed << std::setprecision(0) << r.p90_latency_us
                  << std::setw(10) << std::fixed << std::setprecision(0) << r.p99_latency_us
                  << std::setw(10) << std::fixed << std::setprecision(0) << r.min_latency_us
                  << std::setw(10) << std::fixed << std::setprecision(0) << r.max_latency_us
                  << std::setw(10) << std::fixed << std::setprecision(1) << r.duration_ms
                  << std::endl;
    }

    std::cout << std::string(110, '-') << std::endl;

    // 找到同步基线
    const BenchmarkResult *sync_baseline = nullptr;
    for (const auto &r : results)
    {
        if (r.mode_name.find("Sync") != std::string::npos)
        {
            sync_baseline = &r;
            break;
        }
    }

    if (sync_baseline && sync_baseline->qps > 0)
    {
        std::cout << "\n  vs Sync baseline:" << std::endl;
        for (const auto &r : results)
        {
            if (&r == sync_baseline)
                continue;
            double qps_gain = (r.qps - sync_baseline->qps) / sync_baseline->qps * 100;
            double lat_gain = (sync_baseline->avg_latency_us - r.avg_latency_us) /
                              sync_baseline->avg_latency_us * 100;
            std::cout << "    " << std::left << std::setw(26) << r.mode_name
                      << " QPS: " << (qps_gain >= 0 ? "+" : "")
                      << std::fixed << std::setprecision(1) << qps_gain << "%"
                      << "   Avg Latency: " << (lat_gain >= 0 ? "+" : "")
                      << std::fixed << std::setprecision(1) << lat_gain << "%"
                      << std::endl;
        }
    }

    std::cout << "\n" << std::string(110, '=') << std::endl;
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char *argv[])
{
    const char *ip = "127.0.0.1";
    int port = 12345;
    int total_req = 1000;
    bool sweep_mode = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc)
            total_req = std::stoi(argv[++i]);
        else if (arg == "-p" && i + 1 < argc)
            port = std::stoi(argv[++i]);
        else if (arg == "--sweep")
            sweep_mode = true;
        else if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  -n <N>      Total requests per test (default: 1000)\n"
                      << "  -p <port>   Server port (default: 12345)\n"
                      << "  --sweep     Sweep multiple concurrency levels\n";
            return 0;
        }
    }

    // 抑制框架日志，避免干扰 benchmark 输出
    setLogLevel(LOG_LEVEL_FATAL);

    std::cout << std::string(60, '=') << std::endl;
    std::cout << "  RPC Benchmark: Coroutine vs Sync" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "  Server      : " << ip << ":" << port << std::endl;
    std::cout << "  Requests    : " << total_req << " per test" << std::endl;
    std::cout << "  Sweep mode  : " << (sweep_mode ? "ON" : "OFF") << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::vector<BenchmarkResult> all_results;

    // ------------------------------------------------------------------
    // Test 1: 同步串行基准线
    // ------------------------------------------------------------------
    {
        std::cout << "\n[Test] Sync serial (baseline)..." << std::endl;
        auto r = run_sync_test(ip, port, total_req);
        print_single_result(r);
        all_results.push_back(r);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // ------------------------------------------------------------------
    // Test 2: 单线程协程并发（多并发级别）
    // ------------------------------------------------------------------
    {
        std::vector<int> levels;
        if (sweep_mode)
        {
            levels = {1, 10, 50, 100};
            // 如果总请求数足够大，加入更高并发级别
            if (total_req >= 5000)
                levels.push_back(500);
            if (total_req >= 10000)
                levels.push_back(1000);
        }
        else
        {
            levels = {10, 100};
        }

        for (int co : levels)
        {
            if (co > total_req)
                break;

            std::cout << "\n[Test] Coroutine 1-thread x " << co << " co..." << std::endl;
            auto r = run_coroutine_test(ip, port, co, total_req, 0);
            print_single_result(r);
            all_results.push_back(r);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    // ------------------------------------------------------------------
    // Test 3: 多线程协程并发
    // ------------------------------------------------------------------
    {
        std::vector<int> levels;
        if (sweep_mode)
        {
            levels = {10, 100};
            if (total_req >= 5000)
                levels.push_back(500);
        }
        else
        {
            levels = {100};
        }

        for (int co : levels)
        {
            if (co > total_req)
                break;

            std::cout << "\n[Test] Coroutine multi-thread x " << co << " co..." << std::endl;
            // tid=-1: 由调度器自动分配到多个 Processor
            auto r = run_coroutine_test(ip, port, co, total_req, -1);
            print_single_result(r);
            all_results.push_back(r);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    // ------------------------------------------------------------------
    // 汇总对比
    // ------------------------------------------------------------------
    print_summary_table(all_results);

    _exit(0);
}
