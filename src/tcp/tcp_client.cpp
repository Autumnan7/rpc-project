#include "../../include/tcp/tcp_client.h"

void TcpClient::connect(const char *ip, int port)
{
    LOG_INFO("the client connection to the server");
    m_client_socket.connect(ip, port);
}

// TODO:后续考虑直接析构
int TcpClient::disconnect()
{
    LOG_INFO("TcpClient: sending FIN (shutdownWrite) on fd: %d", m_client_socket.fd());
    // 仅仅发送 FIN 包，状态变为 FIN_WAIT_1
    // 此时 fd 并没有关闭，TcpClient 依然持有这个 Socket
    return m_client_socket.shutdownWrite();
}
ssize_t TcpClient::recv(void *buf, size_t count)
{
    LOG_INFO("enter the tcpclient recv");
    return m_client_socket.read(buf, count);
}
ssize_t TcpClient::send(const void *buf, size_t count)
{
    LOG_INFO("enter the tcpclient send");
    return m_client_socket.send(buf, count);
}