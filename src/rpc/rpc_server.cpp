#include "rpc/rpc_server.h"
#include "rpc/rpc_header.h"

void RpcServer::start(std::string_view ip, int port)
{
    m_rpc_server_stub->register_connection([this](minico::Socket conn)
                                           { this->on_connection(conn); });
    LOG_INFO("register the rpc-server-stub connection callback");
    m_rpc_server_stub->start(ip, port);
}

void RpcServer::start_multi(std::string_view ip, int port, bool bind_thread)
{
    m_rpc_server_stub->register_connection([this](minico::Socket conn)
                                           { this->on_connection(conn); });
    LOG_INFO("register the rpc-server-stub connection callback");
    m_rpc_server_stub->start_multi(ip, port, bind_thread);
}

void RpcServer::on_connection(minico::Socket conn)
{

    RpcHeader rpc_header;
    std::vector<char> buf;

    int rpc_recv_message_len = 0;

    // 循环处理当前连接上的所有 RPC 请求 (支持长连接)
    // 通信协议：8 字节定长 RpcHeader + 变长 JSON Payload，以此解决 TCP 粘包问题
    // 处理流程：读取 Header -> 根据 len 读取 Payload -> 业务路由 -> 编码并回写
    while (true)
    {
        TinyJson request;
        TinyJson response;

        // 1. 读取8字节头部
        int rpc_request_message_len = conn.read(&rpc_header, sizeof(rpc_header));

        // 如果客户端优雅退出，或者发生异常(如RST)返回 < 0，统一断开连接
        if (rpc_request_message_len <= 0)
        {
            LOG_INFO("client disconnected or network error, closing connection.");
            break;
        }

        // 2. 协议安全防线：校验魔数（假设你的框架魔数是 0x7777）
        uint16_t magic = ntohs(rpc_header.magic);
        if (magic != 0x7777)
        {
            LOG_ERROR("invalid magic number");
            break;
        }

        // 3. 字节序转换与防 OOM 处理
        rpc_recv_message_len = ntohl(rpc_header.len);
        if (rpc_recv_message_len <= 0 || rpc_recv_message_len > 10 * 1024 * 1024) // 限制最大包为 10MB
        {
            LOG_ERROR("invalid payload length: %d, possible attack or malformed packet", rpc_recv_message_len);
            break;
        }

        // 4. 重置缓冲区并读取载荷
        // 性能注意点：为避免 vector resize 带来的零初始化(memset)开销，这里只需确保空间足即可
        if (buf.size() < (size_t)rpc_recv_message_len)
        {
            buf.resize(rpc_recv_message_len);
        }

        int payload_read_len = conn.read((void *)&buf[0], rpc_recv_message_len);
        if (payload_read_len != rpc_recv_message_len)
        {
            LOG_ERROR("read payload failed, dropped the packet");
            break;
        }

        // 解码-> 处理 -> 编码
        m_rpc_server_stub->decode(buf, request);

        process(request, response);

        m_rpc_server_stub->encode(response, buf);

        // 发送数据
        conn.send((void *)&buf[0], buf.size());
    }
}

void RpcServer::process(TinyJson &request, TinyJson &response)
{
    /** 解析客户端传入的服务key-value,获取对应的服务名称*/
    std::string service = request.Get<std::string>("service");
    LOG_INFO("the request service is %s", service.c_str());

    if (!service.empty())
    {
        /** 通过服务的名称在服务的注册表中找到对应的服务类*/
        auto s = this->find_service(service);
        if (s)
        {
            LOG_INFO("find the %s -service", s->name());
            s->process(request, response);
        }
        else
        {
            response["err"].Set(404);
            response["errmsg"].Set("service not found");
        }
    }
    else
    {
        response["err"].Set(400);
        response["errmsg"].Set("Request missing 'service' field");
    }
    return;
}
