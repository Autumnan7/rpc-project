#include "rpc/rpc_server_stub.h"

#include <cstring>
#include <string_view>


void RpcServerStub::start(std::string_view ip, int port) {
    /** 开启tcp服务器运行*/
    m_tcp_server->start(ip, port);
    LOG_INFO("rpc-server-stub start run the tcp-server loop");
}

void RpcServerStub::start_multi(std::string_view ip, int port) {
    m_tcp_server->start_multi(ip, port);
    LOG_INFO("rpc-server-stub start run the tcp-server multi loop");
}

void RpcServerStub::register_connection(std::function<void(minico::Socket)> conn) {
    m_tcp_server->register_connection(conn);
}


void RpcServerStub::decode(std::vector<char> &buf, TinyJson &request) {
    /** 将接收到的rpc请求从字节流转换为一个json对象*/
    std::string str_json_request(buf.begin(), buf.end());
    LOG_INFO("the rpc-server-stub received message is %s",
             str_json_request.c_str());

    /** 编码形成一个json对象*/
    request.ReadJson(str_json_request);
}


void RpcServerStub::encode(TinyJson &response, std::vector<char> &buf) {
    std::string str_json_response = response.WriteJson();
    LOG_INFO("the processed rpc message result is %s",
             str_json_response.c_str());
    const size_t body_len = str_json_response.length();

    /** send byte stream*/
    buf.clear();
    buf.resize(sizeof(RpcHeader) + body_len);

    /** 在发送缓冲区中填入rpc头部信息*/
    // 头部的len代表的就是载荷的长度
    // buf.data 也是第一个元素地址
    set_rpc_header(static_cast<void *>(buf.data()), body_len);

    std::memcpy(buf.data() + sizeof(RpcHeader), str_json_response.data(), str_json_response.size());
}

