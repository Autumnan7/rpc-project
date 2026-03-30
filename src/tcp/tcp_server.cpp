#include "../../include/tcp/tcp_server.h"

TcpServer::TcpServer()
    : _on_server_connection(nullptr),
      server_port(-1) {
    LOG_INFO("TcpServer: Instance initialized.");
}

// 这里的析构函数其实不需要手动 delete 任何东西，RAII 会帮你处理。
TcpServer::~TcpServer() {
    LOG_INFO("TcpServer: Instance destroyed, resources released by RAII.");
}

// 3. 注册函数实现
void TcpServer::register_connection(conn_callback func) {
    _on_server_connection = std::move(func); // 使用 move 减少一次 std::function 拷贝
    LOG_INFO("TcpServer: Custom connection callback registered.");
}

/**
 * @brief 默认的连接处理回调函数 (Echo 回显服务器)
 * @note 当用户未注册自定义业务逻辑时，服务器默认执行此函数：读取什么就原样发回什么
 */
TcpServer::conn_callback default_connection =
        [](minico::Socket connect_socket) {
    LOG_INFO("add one client connection, fd: %d", connect_socket.fd());

    // 预分配读缓冲区，避免在循环内频繁申请内存
    std::vector<char> buf;
    buf.resize(2048);

    while (true) {
        LOG_INFO("--------start one read-write process loop------------");

        // 协程阻塞读：若无数据，协程在此挂起让出CPU，有数据时 epoll 唤醒并恢复执行
        auto readNum = connect_socket.read((void *) buf.data(), buf.size());

        // read返回0表示对端关闭连接，返回负数表示发生错误，均需退出循环
        if (readNum <= 0) {
            break;
        }

        // 将读到的数据原样发回客户端
        connect_socket.send((void *) buf.data(), readNum);

        /**
         * @note RPC业务踩坑记录：
         * 在纯粹的RPC短包场景下，如果客户端发完一个请求就不再发包(或断开)，
         * 而此时 readNum < buf.size()，当前协程会死卡在下一次的 read() 上变成僵尸协程。
         *
         * 曾经考虑通过判断 readNum < buf.size() 来直接 break 退出，
         * 但这种暴力做法会导致大包(如超过2048字节)被截断丢失，因此作废。
         *
         * 结论：彻底解决此问题需要在上层业务引入协议解析(如定长包头等)，
         * 而不能在底层的默认回显函数中靠猜测报文边界来退出。
         */
        // if(readNum < (int)buf.size())
        // {
        //     break;
        // }

        LOG_INFO("--------finish one read-write process loop------------");
    }

    // 跳出循环后函数结束，connect_socket 局部对象析构，内部 _refCount 减 1 变为 0，安全关闭 fd 并释放内存
};

/**
 * @brief 启动单线程模式服务器
 * @note 执行流程: 检查回调 -> 创建Socket -> 配置选项 -> bind/listen -> 启动 accept 协程
 */
void TcpServer::start(std::string_view ip, int port) {
    // 如果用户没有注册自定义连接函数，那么就使用默认的
    if (!_on_server_connection) {
        LOG_INFO("user has not registered the connection func, "
            "so tcp_connection func is the default");

        register_connection(default_connection);
    }

    // 创建一个socket，进行服务器的参数配置（使用智能指针管理防止异常路径内存泄漏）
    // 注意：这里用 unique_ptr 管 Listen_fd 是没问题的，因为 Listen_fd 不需要被拷贝共享！
    _listen_fd = std::make_unique<minico::Socket>();
    if (_listen_fd->isUseful()) {
        LOG_INFO("the server listen fd %d is useful", _listen_fd->fd());
        _listen_fd->setTcpNoDelay(true);
        _listen_fd->setReuseAddr(true);
        _listen_fd->setReusePort(true);

        if (_listen_fd->bind(ip.data(), port) < 0) {
            LOG_ERROR("server bind error");
            return; // 智能指针会自动释放资源
        }

        // 开启监听
        _listen_fd->listen();

        // 保存服务器的ip和端口号
        server_ip = (ip != nullptr) ? ip : "any address";
        server_port = port;

        LOG_INFO("server ip is %s", server_ip.c_str());
        LOG_INFO("server port is %d", server_port);
    }

    // start方法需要非阻塞，因此开启一个新协程来运行 server_loop
    minico::co_go([this]() { this->server_loop(); });
}

/**
 * @brief 启动多核模式服务器
 * @note 基于 SO_REUSEPORT 特性，每个 CPU 核心创建一个独立的监听 Socket 和 accept 协程，由内核层面实现负载均衡，避免全局锁竞争
 */
void TcpServer::start_multi(std::string_view ip, int port) {
    // 获取当前系统的 CPU 核心数
    auto tCnt = ::get_nprocs_conf();

    // 如果用户没有注册自定义连接函数，那么就使用默认的
    if (!_on_server_connection) {
        LOG_INFO("user has not registered the connection func, "
            "so tcp_connection func is the default");
        register_connection(default_connection);
    }

    // 使用 vector 替代裸数组 new，自动管理内存，防止异常或中途 return 导致的内存泄漏
    _multi_listen_fd.resize(tCnt);

    // 每个核心对应一个独立的监听和运行循环
    for (int i = 0; i < tCnt; ++i) {
        if (!_multi_listen_fd[i].isUseful()) {
            LOG_ERROR("create socket failed on core %d, maybe reach fd limit?", i);
            return;
        }

        LOG_INFO("the tcpserver listen fd is useful on core %d", i);
        _multi_listen_fd[i].setTcpNoDelay(true);
        _multi_listen_fd[i].setReuseAddr(true);
        // 多核模式下必须开启 SO_REUSEPORT，允许多个 fd 绑定同一个 IP+Port
        _multi_listen_fd[i].setReusePort(true);

        if (_multi_listen_fd[i].bind(ip.data(), port) < 0) {
            LOG_ERROR("multi server bind error on core %d", i);
            return; // bind 失败直接终止启动，vector 析构会自动清理前面已创建的 Socket
        }

        // 开启监听
        _multi_listen_fd[i].listen();

        // 使用 Lambda 替代 std::bind，避免 std::function 隐式转换带来的额外堆内存开销
        // 捕获当前核心号 i，开启协程并绑定到指定的 CPU 核心上运行
        minico::co_go([this, i]() { this->multi_server_loop(i); },
                      minico::parameter::coroutineStackSize, i);
    }
}

/**
 * @brief 单核模式下的主循环
 * @note 负责不断 accept 新连接，并为每个新连接派发独立的业务处理协程
 */
void TcpServer::server_loop() {
    LOG_INFO("-------------------------");
    LOG_INFO("start run the server loop");
    LOG_INFO("-------------------------");

    while (true) {
        LOG_INFO("block in server_loop,has no new client accept");

        // accept 返回一个封装好的 Socket 对象（内部持有 conn_fd）
        minico::Socket conn(_listen_fd->accept());

        // 检查 accept 返回的对象是否合法（防止返回无效 fd 包装出的空壳对象）
        if (!conn.isUseful()) {
            LOG_ERROR("accept error, get an invalid socket");
            continue;
        }

        LOG_INFO("unblock,the server add a new tcpclient connection,the connect fd is %d", conn.fd());
        conn.setTcpNoDelay(true);

        // 拷贝一份回调函数，防止协程执行期间外部修改回调导致线程安全问题
        auto user_connection = _on_server_connection;

        // server_loop 循环末尾栈上的 conn 析构，_refCount 变 1；新协程持有 _refCount 为 1 的 conn
        minico::co_go([this, conn]() mutable { this->_on_server_connection(std::move(conn)); });
    }
}

/**
 *@brief 服务器多核工作函数（绑定在指定 CPU 核心上运行）
 */
void TcpServer::multi_server_loop(int thread_number) {
    LOG_INFO("start run the multi server loop on core %d", thread_number);

    while (true) {
        // 基于 SO_REUSEPORT，每个核独立 accept，由内核做负载均衡
        minico::Socket conn(_multi_listen_fd[thread_number].accept());

        if (!conn.isUseful()) {
            LOG_ERROR("multi server loop accept error on core %d", thread_number);
            continue;
        }

        LOG_INFO("core %d add one client socket, fd is %d", thread_number, conn.fd());
        conn.setTcpNoDelay(true);

        minico::co_go([this, conn]() mutable {
            // std::move(conn) 只是把 Lambda 捕获的那个副本“推”进回调函数
            // 不会增加计数，也不会提前析构，性能更优
            this->_on_server_connection(std::move(conn));
        });
    }
}
