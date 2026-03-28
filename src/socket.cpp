#include "socket.h"
#include "scheduler.h"
#include "logger.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>

using namespace minico;

/** RAII 析构函数 */
Socket::~Socket()
{
	// 使用引用计数 _refCount 管理 fd 的生命周期
	// 只有当引用计数为 1（即当前对象是最后一个持有者）且 Socket 有效时，才真正关闭 fd
	if (_refCount && _refCount.use_count() == 1 && isUseful())
	{
		::close(_sockfd);
		_sockfd = -1;
	}
}

/** 获取 TCP 底层信息 */
bool Socket::getSocketOpt(struct tcp_info *tcpi) const
{
	socklen_t len = sizeof(*tcpi);
	memset(tcpi, 0, sizeof(*tcpi)); // 清空结构体，防止未初始化的字段导致错误
	return ::getsockopt(_sockfd, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

/** 获取 TCP 信息并格式化为字符串 */
bool Socket::getSocketOptString(char *buf, int len) const
{
	struct tcp_info tcpi;
	bool ok = getSocketOpt(&tcpi);
	if (ok)
	{
		// 将 tcp_info 结构体中的关键字段格式化写入 buf
		int ret = snprintf(buf, len, "unrecovered=%u "
									 "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
									 "lost=%u retrans=%u rtt=%u rttvar=%u "
									 "ssthresh=%u cwnd=%u total_retrans=%u",
						   tcpi.tcpi_retransmits, // 未恢复的超时次数
						   tcpi.tcpi_rto,		  // 重传超时时间
						   tcpi.tcpi_ato,		  // 预测的软时钟
						   tcpi.tcpi_snd_mss,
						   tcpi.tcpi_rcv_mss,
						   tcpi.tcpi_lost,	  // 丢包数
						   tcpi.tcpi_retrans, // 重传包数
						   tcpi.tcpi_rtt,	  // 平滑往返时间
						   tcpi.tcpi_rttvar,  // 往返时间方差
						   tcpi.tcpi_snd_ssthresh,
						   tcpi.tcpi_snd_cwnd,
						   tcpi.tcpi_total_retrans); // 总重传次数
		if (ret < 0 || ret >= len)
		{
			// 写入失败或缓冲区截断
			return false;
		}
	}

	return ok;
}

std::string Socket::getSocketOptString() const
{
	// 栈上分配缓冲区
	char buf[1024];

	// 调用底层实现
	if (getSocketOptString(buf, sizeof(buf)))
	{
		return std::string(buf);
	}

	// 失败时返回错误信息
	return "Failed to get socket options or buffer truncated";
}

/** 绑定 IP 和端口 */
int Socket::bind(const char *ip, int port)
{
	_port = port;
	struct sockaddr_in serv; // 服务端地址结构
	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;	 // IPv4
	serv.sin_port = htons(port); // 主机序转网络序

	if (ip == nullptr)
	{
		// IP 为空，绑定所有网卡 (0.0.0.0)
		serv.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		// 绑定指定 IP
		serv.sin_addr.s_addr = inet_addr(ip);
	}

	int ret = ::bind(_sockfd, (struct sockaddr *)&serv, sizeof(serv));
	return ret;
}

/** 监听端口 */
int Socket::listen()
{
	int ret = ::listen(_sockfd, parameter::backLog);
	return ret;
}

/** 原始 accept：调用系统调用，不包含协程逻辑 */
Socket Socket::accept_raw()
{
	int connfd = -1;
	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	// 系统调用 accept，获取客户端连接
	connfd = ::accept(_sockfd, (struct sockaddr *)&client, &len);

	if (connfd < 0)
	{
		// 连接失败，返回无效 Socket
		return Socket(connfd);
	}

	// 解析客户端 IP 和端口
	int port = ntohs(client.sin_port); // 网络序转主机序

	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip)); // 二进制 IP 转字符串

	return Socket(connfd, std::string(ip), port);
}

/** 协程版 accept：封装了异步等待逻辑 */
Socket Socket::accept()
{
	// 1. 尝试直接获取连接
	auto ret(accept_raw());
	if (ret.isUseful())
	{
		return ret;
	}

	// 2. 如果没有连接，则加入 epoll 监听池，并挂起当前协程 (yield)
	// 监听事件：可读(新连接)、紧急数据、挂起、错误
	minico::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);

	// 3. 协程恢复运行，说明有事件到来，再次尝试 accept
	auto con(accept_raw());
	if (con.isUseful())
	{
		return con;
	}

	// 4. 极少数情况：唤醒了但连接失败（如竞争），递归重试
	return accept();
}

/** 协程版 read：封装了异步等待逻辑 */
ssize_t Socket::read(void *buf, size_t count)
{
	// 调用系统接口读取数据
	auto ret = ::read(_sockfd, buf, count);

	if (ret >= 0)
	{
		// 读取成功（或对端关闭），直接返回
		return ret;
	}

	// 处理被信号中断的情况
	if (ret == -1 && errno == EINTR)
	{
		LOG_INFO("read has error");
		return read(buf, count);
	}

	// 数据未就绪 (EAGAIN/EWOULDBLOCK)，挂起协程等待 EPOLLIN 事件
	minico::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);

	// 协程恢复后，递归调用 read 继续尝试读取
	return read(buf, count);
}

/** 客户端连接服务器 */
void Socket::connect(const char *ip, int port)
{
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &addr.sin_addr);
	_ip = std::string(ip);
	_port = port;

	// 非阻塞 connect 通常返回 -1 (EINPROGRESS)
	auto ret = ::connect(_sockfd, (struct sockaddr *)&addr, sizeof(sockaddr_in));

	if (ret == 0)
	{
		// 立即连接成功（罕见）
		return;
	}

	if (ret == -1 && errno == EINTR)
	{
		// 被信号中断，重试
		return connect(ip, port);
	}

	// 等待 socket 可写事件，表示连接建立完成或出错，一旦三次握手完成，内核会触发 EPOLLOUT 事件
	minico::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLOUT);

	// 恢复后再次尝试 connect (通常是为了检查连接状态或处理逻辑)
	return connect(ip, port);
}

/** 协程版 send：循环发送直到完成 */
ssize_t Socket::send(const void *buf, size_t count)
{
	// 尝试发送数据，MSG_NOSIGNAL 防止对端关闭导致进程退出
	size_t sendIdx = ::send(_sockfd, buf, count, MSG_NOSIGNAL);

	if (sendIdx >= count)
	{
		// 全部发送完成
		return count;
	}

	// 缓冲区满，等待 socket 可写事件
	minico::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLOUT);

	// 恢复后，偏移指针继续发送剩余部分
	return send((char *)buf + sendIdx, count - sendIdx);
}

/** 关闭写端 */
int Socket::shutdownWrite()
{
	int ret = ::shutdown(_sockfd, SHUT_WR); // 我已经把话说完了（发 FIN），但你还可以继续说，我还在听。
	return ret;
}

/** 设置 TCP_NODELAY (禁止 Nagle 算法，降低延迟) */
int Socket::setTcpNoDelay(bool on)
{
	int optval = on ? 1 : 0;
	int ret = ::setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY,
						   &optval, static_cast<socklen_t>(sizeof optval));
	return ret;
}

/** 设置地址复用 */
int Socket::setReuseAddr(bool on)
{
	int optval = on ? 1 : 0;
	int ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
						   &optval, static_cast<socklen_t>(sizeof optval));
	return ret;
}

/** 设置端口复用 */
int Socket::setReusePort(bool on)
{
	int ret = -1;
#ifdef SO_REUSEPORT
	int optval = on ? 1 : 0;
	ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT,
					   &optval, static_cast<socklen_t>(sizeof optval));
#endif
	return ret;
}

/** 设置 Keep-Alive 保活机制 */
int Socket::setKeepAlive(bool on)
{
	int optval = on ? 1 : 0;
	int ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE,
						   &optval, static_cast<socklen_t>(sizeof optval));
	return ret;
}

/** 设置非阻塞 Socket */
int Socket::setNonBlockSocket()
{
	auto flags = fcntl(_sockfd, F_GETFL, 0);
	int ret = fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);
	return ret;
}

/** 设置阻塞 Socket */
int Socket::setBlockSocket()
{
	auto flags = fcntl(_sockfd, F_GETFL, 0);
	int ret = fcntl(_sockfd, F_SETFL, flags & ~O_NONBLOCK);
	return ret;
}

/** 协程版 recvfrom：UDP 接收 */
ssize_t Socket::recvfrom(int sockfd, void *buf, int len, unsigned int flags,
						 sockaddr *from, socklen_t *fromlen)
{
	// 1. 校验 Socket 描述符
	if (sockfd != _sockfd)
	{
		LOG_INFO("ERROR: the sockfd is not same as current Socket");
		errno = EINVAL; // 设置错误码，规范化
		return -1;
	}

	// 2. 使用 while 循环处理重试逻辑
	while (true)
	{
		ssize_t ret = ::recvfrom(sockfd, buf, len, flags, from, fromlen);

		// 成功情况：收到数据（或对端关闭返回0）
		if (ret >= 0)
		{
			return ret;
		}

		// 错误情况处理
		// 情况 A：被信号中断，直接重试
		if (errno == EINTR)
		{
			continue;
		}

		// 情况 B：资源暂不可用（非阻塞模式正常行为），挂起协程等待
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// 等待读事件，EPOLLIN 是必须的，其他事件视需求而定
			minico::Scheduler::getScheduler()
				->getProcessor(threadIdx)
				->waitEvent(sockfd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);

			// 协程恢复后，continue 回到 while 开头继续尝试读取
			continue;
		}

		// 情况 C：其他无法恢复的错误，直接返回失败
		return -1;
	}
}

/** 协程版 sendto：UDP 发送 */
ssize_t Socket::sendto(int sockfd, const void *buf, int len, unsigned int flags,
					   const struct sockaddr *to, int tolen)
{
	// 1. 校验 Socket 描述符
	if (sockfd != _sockfd)
	{
		LOG_INFO("ERROR: the sockfd is not same as current Socket");
		errno = EINVAL;
		return -1;
	}

	// 2. 准备发送循环变量
	// 关键修正：使用 const char* 指针偏移，避免递归传参时的类型转换风险
	const char *p = static_cast<const char *>(buf);
	int left = len;

	// 3. 循环发送，直到发完或出错
	while (left > 0)
	{
		ssize_t n = ::sendto(sockfd, p, left, flags, to, tolen);

		// 情况 A：发送成功（可能是部分发送）
		if (n > 0)
		{
			p += n;						 // 指针后移
			left -= static_cast<int>(n); // 更新剩余长度
			continue;					 // 继续 try，如果 left=0 循环结束
		}

		// 情况 B：信号中断
		if (n == -1 && errno == EINTR)
		{
			continue;
		}

		// 情况 C：缓冲区满，挂起协程等待可写事件
		// EAGAIN / EWOULDBLOCK  资源暂时不可用
		if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			minico::Scheduler::getScheduler()
				->getProcessor(threadIdx)
				->waitEvent(sockfd, EPOLLOUT);

			// 协程恢复后，continue 重试发送剩余数据 (p 和 left 已更新)
			continue;
		}

		// 情况 D：其他错误
		return -1;
	}

	// 全部发送完毕
	return len;
}
