#pragma once

#include <functional>
#include <sys/sysinfo.h>

#include "../../include/logger.h"
#include "../../include/socket.h"
#include "../../include/minico_api.h"

/**
 * @brief: 客户端 一个客户端只用来保存一个连接 客户端必须在一个协程中运行
 */
class TcpClient
{
public:
    /**
     * @brief 默认构造：直接构造 Socket 对象。
     * @note 由于 Socket 内部有 _refCount，配合 DISALLOW_COPY，直接按值存储最安全。
     */
    TcpClient() : m_client_socket()
    {
        LOG_INFO("tcpclient constructor a connection socket");
    }

    DISALLOW_COPY_MOVE_AND_ASSIGN(TcpClient);

    virtual ~TcpClient()
    {
        LOG_INFO("tcpclient destructor itself and the connection socket");
    }

    /**
     * @brief 协程安全：连接服务器
     * @note 若连接未立即建立，当前协程会挂起，不阻塞工作线程。
     */
    void connect(const char *ip, int port);

    /**
     * @brief 断开当前连接
     * @return 0: 成功断开并关闭 fd; -1: 断开过程发生错误
     */
    int disconnect();

    /**
     * @brief 从连接中读取数据（协程安全）
     * @param buf   接收数据的缓冲区首地址
     * @param count 期望读取的最大字节数
     * @return 实际读取到的字节数 (若返回 0 表示对端已关闭连接，返回 <0 表示出错)
     * @note 【协程语义】如果当前 socket 缓冲区无数据可读，当前协程会自动 yield 挂起，
     *       由 epoll 监听可读事件，数据到达后自动恢复执行。对外表现为同步代码，内部为异步事件。
     */
    ssize_t recv(void *buf, size_t count);

    /**
     * @brief 向连接中发送数据（协程安全）
     * @param buf   待发送数据的缓冲区首地址
     * @param count 期望发送的字节数
     * @return 实际发送的字节数 (返回 <0 表示出错)
     * @note 【协程语义】如果当前 socket 发送缓冲区已满导致无法立即写入，当前协程会自动 yield 挂起，
     *       由 epoll 监听可写事件，缓冲区有空闲后自动恢复发送。
     */
    ssize_t send(const void *buf, size_t count);

    /**
     * @brief 获取底层的文件描述符
     * @return 当前连接的 fd
     * @note 仅在需要绑定到 epoll 或进行特殊系统调用时使用，一般情况下直接使用类提供的接口即可。
     */
    inline int socket() const { return m_client_socket.fd(); }

private:
    // 拥有该 Socket 的独立所有权，析构时自动 close(fd)
    minico::Socket m_client_socket;
};
