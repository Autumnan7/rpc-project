#pragma once

#include <stdint.h>
#include <cstddef>

/**
 * @brief RPC 协议头部结构 (固定 8 字节)
 * 为了解决 TCP 粘包/半包问题，必须在发送 JSON 数据前先发送这个头部。
 */
struct RpcHeader {
    /** * 消息类型或版本信息 (2 bytes)
     * 可以用来区分是 Request、Response 还是 心跳包(Heartbeat)
     */
    uint16_t info;

    /** * 魔数 (2 bytes)
     * 常用固定值(如 0xABCD)。接收方收到包后先看魔数，
     * 如果不对，说明不是合法的 RPC 消息，直接断开连接，防止被非法攻击或误解析。
     */
    uint16_t magic;

    /** * RPC 消息体(Json数据)的长度 (4 bytes)
     * 接收方先读取 8 字节头部，解析出 len，然后再精准读取 len 长度的字节作为 Json 内容。
     */
    uint32_t len;
};

/**
 * @brief 填充 RPC 头部信息
 * @param header 指向头部内存的指针
 * @param msg_len 后续紧跟的 Json 字符串长度
 */
void set_rpc_header(void *header, size_t msg_len);
