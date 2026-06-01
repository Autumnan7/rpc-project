#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <string_view>
#include <sys/sysinfo.h>

#include "../../include/logger.h"
#include "../../include/socket.h"
#include "../../include/minico_api.h"

/**
 * @brief 基于协程的 TCP 服务器基类
 * @note 采用 Reactor 模式，主协程负责 accept，新连接派发独立协程处理
 */
class TcpServer {
public:
    /** @brief 客户端连接处理回调函数类型 */
    using conn_callback = std::function<void(minico::Socket)>;

    TcpServer();

    virtual ~TcpServer();

    /**
     * @brief 启动单线程模式服务器
     * @param ip   监听的 IP 地址
     * @param port 监听的端口号
     */
    void start(std::string_view ip, int port);

    /**
     * @brief 启动多线程模式服务器 (基于 SO_REUSEPORT)
     * @param ip          监听的 IP 地址
     * @param port        监听的端口号
     * @param bind_thread 是否将业务协程绑定到 accept 所在核心（true=绑定，false=走全局负载均衡）
     */
    void start_multi(std::string_view ip, int port, bool bind_thread = true);

    /**
     * @brief 注册连接到达后的业务处理回调
     * @param func 用户自定义的连接处理函数
     */
    void register_connection(conn_callback func);

private:
    /** @brief 单核主循环：负责不断 accept 新连接 */
    void server_loop();

    /** @brief 多核主循环：指定线程号的主循环 */
    void multi_server_loop(int thread_number, bool bind_thread);

    /** @brief 用户注册的业务回调函数 */
    conn_callback _on_server_connection;

    /** @brief 单线程模式下的监听 Socket */
    std::unique_ptr<minico::Socket> _listen_fd;

    /** @brief 多线程模式下的监听 Socket 数组 */
    std::vector<minico::Socket> _multi_listen_fd;

    /** @brief 服务器绑定的 IP 地址 */
    std::string server_ip;

    /** @brief 服务器绑定的端口号 */
    int server_port;
};
