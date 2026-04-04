/**
 * @file rpc_client.cpp
 * @brief RPC 客户端示例 - 演示服务调用与响应处理
 *
 * 本示例展示如何使用 RpcClient 调用远程服务：
 * - 连接 RPC 服务器
 * - 构造请求并调用服务方法
 * - 解析响应结果
 */

#include <iostream>
#include <string>

#include "logger.h"
#include "rpc/rpc_client.h"

/**
 * @brief RPC 客户端工作协程
 * @param rpc_client RPC 客户端实例
 * @param count      调用次数
 *
 * 演示完整的服务调用流程：
 * 1. 建立连接
 * 2. 心跳检测
 * 3. 调用 GetStatus 和 GetMetrics 方法
 */
void rpc_client_worker(RpcClient &rpc_client, int count)
{
    rpc_client.connect("127.0.0.1", 12345);

    for (int i = 0; i < count; ++i)
    {
        LOG_INFO("========== Round %d ==========", i + 1);

        // 心跳检测
        // rpc_client.ping();

        // 调用 GetStatus 方法
        {
            TinyJson request;
            TinyJson response;

            request["service"].Set<std::string>("SystemMonitorService");
            request["method"].Set<std::string>("GetStatus");

            rpc_client.call(request, response);

            int status = response.Get<int>("status", -1);
            LOG_INFO("[GetStatus] status: %d", status);
        }

        // 调用 GetMetrics 方法
        {
            TinyJson request;
            TinyJson response;

            request["service"].Set<std::string>("SystemMonitorService");
            request["method"].Set<std::string>("GetMetrics");

            rpc_client.call(request, response);

            int status = response.Get<int>("status", -1);
            LOG_INFO("[GetMetrics] status: %d", status);
        }
    }
}

int main()
{
    LOG_INFO("[System] Starting RPC Client...");

    RpcClient rpc_client;
    int call_count = 100;

    minico::co_go([&rpc_client, call_count]()
                  { rpc_client_worker(rpc_client, call_count); });

    minico::sche_join();

    return 0;
}
