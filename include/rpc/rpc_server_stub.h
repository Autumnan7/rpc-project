// 客户端存根：将本地调用转换为网络请求
#pragma once

#include "rpc/rpc_header.h"
#include "tcp/tcp_server.h"
#include "json.h"
#include <string_view>

//前向声明
class RpcServer;

class RpcServerStub {
public:
    DISALLOW_COPY_MOVE_AND_ASSIGN(RpcServerStub);

    RpcServerStub() : m_tcp_server(new TcpServer()) {
        LOG_INFO("rpc server constructor a tcp server");
    }

    ~RpcServerStub() {
        delete m_tcp_server;
        m_tcp_server = nullptr;
        LOG_INFO("rpc_client destructor a tcp server");
    }

    /**
    * @brief 启动 RPC 服务端（单线程/单 Reactor 模式）
    * @param ip   监听的 IP 地址
    * @param port 监听的端口号
    */
    void start(std::string_view ip, int port);

    /**
     * @brief 启动 RPC 服务端（多线程/多 Reactor 模式）
     * @param ip   监听的 IP 地址
     * @param port 监听的端口号
     */
    void start_multi(std::string_view ip, int port);

    /**
     * @brief 注册新连接建立后的回调函数
     * @param conn 回调函数，参数为新建立的 Socket 指针
     */
    void register_connection(std::function<void(minico::Socket)> conn);


    void encode(TinyJson &response, std::vector<char> &buf);

    /**
     * @brief 将字节流数据解码为 JSON 请求对象
     * @note 这里的逻辑本质是反序列化 (Deserialization)
     * @param buf     接收到的字节流缓冲区
     * @param request 输出参数，解析后的 JSON 对象
     */
    void decode(std::vector<char> &buf, TinyJson &request);


    void process(TinyJson &request, TinyJson &result);

private:
    /** connection callback*/
    void on_connection(minico::Socket *conn);

    TcpServer *m_tcp_server;
};
