/**
 * @file rpc_server.cpp
 * @brief RPC 服务器示例 - 演示基于 ServiceDispatcher 模式的服务注册与请求路由
 *
 * 本示例展示如何使用 Dispatcher 模式构建可扩展的 RPC 服务：
 * - ServiceDispatcher 基类提供方法路由能力
 * - 具体服务继承基类实现业务逻辑
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <sys/sysinfo.h>

#include "logger.h"
#include "rpc/rpc_server.h"

/**
 * @brief 服务分发器基类
 *
 * 采用 Dispatcher 模式，将请求按方法名分发到对应的处理函数。
 * 子类只需实现具体的业务处理方法，无需关心路由逻辑。
 */
class ServiceDispatcher : public Service
{
public:
    /// 方法处理器类型：指向成员函数的指针
    using MethodHandler = void (ServiceDispatcher::*)(TinyJson &request, TinyJson &response);

    /**
     * @brief 构造函数
     * @param service_name 服务名称，用于 RPC 注册与查找
     */
    explicit ServiceDispatcher(const std::string &service_name)
        : service_name_(service_name)
    {
        // 注册方法处理映射
        handlers_["GetStatus"] = &ServiceDispatcher::OnGetStatus;
        handlers_["GetMetrics"] = &ServiceDispatcher::OnGetMetrics;
    }

    ~ServiceDispatcher() override = default;

    /**
     * @brief 获取服务名称
     * @return 服务名称的 C 字符串指针
     */
    const char *name() const override
    {
        return service_name_.c_str();
    }

    /**
     * @brief 处理 RPC 请求（核心路由逻辑）
     * @param request  请求 JSON 对象
     * @param response 响应 JSON 对象
     *
     * 根据 method 字段查找并调用对应的处理方法，
     * 若方法不存在则返回错误响应。
     */
    void process(TinyJson &request, TinyJson &response) override
    {
        std::string method = request.Get<std::string>("method");
        LOG_INFO("[RPC] Dispatching method: %s", method.c_str());

        if (method.empty())
        {
            SetError(response, 400, "Method not specified");
            return;
        }

        auto it = handlers_.find(method);
        if (it == handlers_.end())
        {
            SetError(response, 404, "Method not found");
            return;
        }

        // 调用具体业务处理方法
        (this->*(it->second))(request, response);
    }

protected:
    /// @name 业务处理接口（子类实现）
    /// @{
    virtual void OnGetStatus(TinyJson &request, TinyJson &response) = 0;
    virtual void OnGetMetrics(TinyJson &request, TinyJson &response) = 0;
    /// @}

    /**
     * @brief 设置错误响应
     * @param res   响应对象
     * @param code  错误码
     * @param msg   错误信息
     */
    void SetError(TinyJson &res, int code, const std::string &msg)
    {
        res["status"].Set(code);
        res["msg"].Set(msg);
    }

private:
    std::unordered_map<std::string, MethodHandler> handlers_; ///< 方法名 -> 处理函数映射
    std::string service_name_;                                ///< 服务名称
};

/**
 * @brief 系统监控服务
 *
 * 提供系统运行状态和性能指标的查询接口，
 * 包括服务健康检查、系统负载、内存使用等信息。
 */
class SystemMonitor : public ServiceDispatcher
{
public:
    SystemMonitor()
        : ServiceDispatcher("SystemMonitorService")
    {
    }

    ~SystemMonitor() override = default;

    /**
     * @brief 获取服务状态
     *
     * 返回服务健康状态和当前时间戳，
     * 用于心跳检测和服务可用性验证。
     */
    void OnGetStatus(TinyJson &request, TinyJson &response) override
    {
        (void)request; // 未使用参数

        response["status"].Set(200);

        TinyJson data;
        data["message"].Set("Service is healthy");
        data["timestamp"].Set(static_cast<int64_t>(time(nullptr)));
        response["data"].Set(data);
    }

    /**
     * @brief 获取系统指标
     *
     * 返回系统运行指标，包括：
     * - uptime: 系统运行时长（秒）
     * - load_1m: 1 分钟平均负载
     * - total_ram: 总内存大小
     */
    void OnGetMetrics(TinyJson &request, TinyJson &response) override
    {
        (void)request; // 未使用参数

        struct sysinfo info;
        sysinfo(&info);

        response["status"].Set(200);

        TinyJson data;
        data["uptime"].Set(static_cast<int64_t>(info.uptime));
        data["load_1m"].Set(static_cast<int64_t>(info.loads[0]));
        data["total_ram"].Set(static_cast<int64_t>(info.totalram));
        response["data"].Set(data);
    }
};

int main(int argc, char *argv[])
{
    bool bind_thread = true; // 默认绑定到 accept 所在核

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--no-bind" || arg == "-n")
        {
            bind_thread = false;
            std::cout << "[Mode] No-bind (global load balancing)" << std::endl;
        }
        else if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  -n, --no-bind  Use global load balancing (default: bind to accept core)\n"
                      << "  -h, --help     Show this help\n";
            return 0;
        }
    }

    LOG_INFO("[System] Starting RPC Server Node (bind_thread=%s)...", bind_thread ? "true" : "false");
    setLogLevel(LOG_LEVEL_FATAL);

    RpcServer rpc_server;
    rpc_server.add_service(new SystemMonitor());
    rpc_server.start_multi("0.0.0.0", 12345, bind_thread);
    minico::sche_join();

    return 0;
}
