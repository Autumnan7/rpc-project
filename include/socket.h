#pragma once

#include "utils.h"
#include "parameter.h"
#include <arpa/inet.h>
#include <sys/types.h>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <memory>

#include "logger.h"

/**
 * hook 的核心价值是：把阻塞 IO 改造成协程可调度的非阻塞 IO
 */

struct tcp_info;

namespace minico
{
	enum class SocketType
	{
		TCP,
		UDP
	};

	/** hook socket*/
	class Socket
	{
	public:
		explicit Socket(int sockfd, std::string ip = "", int port = -1)
			: _sockfd(sockfd), _refCount(std::make_shared<int>(1)), _port(port), _ip(std::move(ip))
		{
			if (sockfd > 0)
			{
				/** socket为非阻塞*/
				setNonBlockSocket();
			}
		}

		// 默认构造函数，默认创建一个TCP Socket
		Socket(SocketType type = SocketType::TCP)
			: _refCount(std::make_shared<int>(1)),
			  _port(-1),
			  _ip("")
		{
			if (type == SocketType::UDP)
			{
				_sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
			}
			else if (type == SocketType::TCP)
			{
				_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
			}
			else
			{
				LOG_INFO("Socket type set error,default set the Socket type to TCP");
				_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
			}
		}

		Socket(const Socket &otherSock) : _sockfd(otherSock._sockfd)
		{
			*(otherSock._refCount) += 1;
			_refCount = otherSock._refCount;
			_ip = otherSock._ip;
			_port = otherSock._port;
		}

		// TODO: 移动构造需要调整
		Socket(Socket &&otherSock) : _sockfd(otherSock._sockfd)
		{
			*(otherSock._refCount) += 1;
			_refCount = otherSock._refCount;
			_ip = std::move(otherSock._ip);
			_port = otherSock._port;
		}

		Socket &operator=(const Socket &otherSock) = delete;

		~Socket();

		// =========================
		// 基础信息查询
		// =========================

		/** 返回当前 Socket 对应的系统 fd */
		int fd() const { return _sockfd; }

		/** 返回当前 Socket 是否可用 */
		bool isUseful() const { return _sockfd >= 0; }

		/** 返回当前 Socket 绑定或记录的 IP 地址 */
		std::string ip() const { return _ip; }

		/** 返回当前 Socket 绑定或记录的端口号 */
		int port() const { return _port; }

		// =========================
		// 连接管理
		// =========================

		/** 将当前 Socket 绑定到指定 ip 和 port */
		int bind(const char *ip, int port);

		/** 将当前 Socket 设置为监听状态 */
		int listen();

		/** 接收一个连接，并返回一个新的 Socket 对象 */
		Socket accept();

		/** 连接到指定 ip 和 port */
		void connect(const char *ip, int port);

		/** 关闭当前 Socket 的写方向 */
		int shutdownWrite();

		// =========================
		// 数据收发
		// =========================

		/** 协程化改造：从 Socket 中读取数据 */
		ssize_t read(void *buf, size_t count);

		/** 协程化改造：向 Socket 发送数据 */
		ssize_t send(const void *buf, size_t count);

		/** UDP 接收数据 */
		ssize_t recvfrom(int sockfd, void *buf, int len, unsigned int flags,
						 sockaddr *from, socklen_t *fromlen);

		/** UDP 发送数据 */
		ssize_t sendto(int sockfd, const void *buf, int len, unsigned int flags,
					   const struct sockaddr *to, int tolen);

		// =========================
		// Socket 配置选项
		// =========================

		/** 获取 Socket 的 tcp_info 选项 */
		bool getSocketOpt(struct tcp_info *) const;

		/** 将 Socket 状态以字符串形式写入缓冲区 */
		bool getSocketOptString(char *buf, int len) const;

		/** 返回 Socket 状态字符串 */
		std::string getSocketOptString() const;

		/** 设置是否开启 Nagle 算法，减少小包延迟 */
		int setTcpNoDelay(bool on);

		/** 设置地址复用 */
		int setReuseAddr(bool on);

		/** 设置端口复用 */
		int setReusePort(bool on);

		/** 设置心跳检测 */
		int setKeepAlive(bool on);

		/** 设置套接字为非阻塞 */
		int setNonBlockSocket();

		/** 设置套接字为阻塞 */
		int setBlockSocket();

	private:
		/** 接收一个连接，并返回一个新的Socket连接的具体实现*/
		Socket accept_raw();

		/** 系统套接字*/
		int _sockfd;

		/** 引用计数*/
		std::shared_ptr<int> _refCount;

		/** 端口号*/
		int _port;

		/** IP地址*/
		std::string _ip;
	};

}
