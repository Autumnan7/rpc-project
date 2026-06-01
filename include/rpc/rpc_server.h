#pragma once

#include <unordered_map>

#include "tcp/tcp_server.h"
#include "rpc/rpc_server_stub.h"
#include "json.h"
#include "rpc/service.h"
#include <string_view>
/**
 * @brief 基于协程的rpc服务器
 * 在复用 TcpServer 的基础上，封装 RPC 服务注册与请求处理逻辑。
 */
class RpcServer
{
public:
    DISALLOW_COPY_MOVE_AND_ASSIGN(RpcServer);

    RpcServer() : m_rpc_server_stub(new RpcServerStub())
    {
        LOG_INFO("rpcserver constructor the rpc-server-stub");
        /** add a ping service*/
        add_service(new Ping);
        LOG_INFO("add a service ping");
    }

    ~RpcServer()
    {
        LOG_INFO("rpcserver destructor the tcpserver");
        delete m_rpc_server_stub;
        m_rpc_server_stub = nullptr;
    };

    // 启动RPC服务器
    void start(std::string_view ip, int port);

    void start_multi(std::string_view ip, int port, bool bind_thread = true);

    // 注册服务
    void add_service(Service *s)
    {
        m_services[s->name()] = std::shared_ptr<Service>(s);
    }

    // 根据服务名进行路由查找
    Service *find_service(const std::string &name)
    {
        auto it = m_services.find(name);
        if (it != m_services.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    // 处理业务逻辑
    void process(TinyJson &request, TinyJson &response);

private:
    /** connection callback*/
    void on_connection(minico::Socket conn);

    /** tcp server handle*/
    RpcServerStub *m_rpc_server_stub;

    std::atomic<int> m_conn_number;

    // RPC 服务注册表
    // 维护 服务名称(service_name) 到 服务实例 的路由映射，并管理 Service 的生命周期
    std::unordered_map<std::string, std::shared_ptr<Service>> m_services;
};
