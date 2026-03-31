# AGENTS.md

本文档为 AI 助手提供项目上下文，帮助理解和操作本代码库。

## 项目概述

这是一个基于 GNU ucontext API 的用户态协程调度框架（mini coroutine），使用 C++17 开发。项目实现了一个高性能的 RPC 框架，包含协程调度、网络通信、序列化等完整功能。

### 核心技术栈
- **语言标准**: C++17
- **构建系统**: CMake 3.16+
- **协程实现**: GNU ucontext API
- **事件驱动**: epoll (Linux)
- **线程支持**: std::thread, std::atomic
- **序列化**: TinyJson (轻量级 JSON 库)

## 构建与运行

```bash
# 创建构建目录并编译
mkdir -p build && cd build
cmake ..
make

# 运行冒烟测试
./bin/processor_smoke_test

# 运行 TCP 服务端测试
./bin/tcp_server_test

# 运行 TCP 客户端测试
./bin/tcp_client_test

# 重新编译并运行
make && ./bin/processor_smoke_test
```

### 输出目录
- 可执行文件: `bin/`
- 库文件: `lib/`

### 编译器支持
- **GCC/Clang**: 使用 `-Wall -Wextra -Wpedantic` 警告选项
- **MSVC**: 使用 `/W4 /permissive-` 警告选项（但代码依赖 Linux API，无法在 Windows 上实际编译）

## 架构概览

项目采用分层架构，自底向上分为以下层次：

### 1. 内存层 (`include/mempool.h`, `include/objpool.h`)
- `MemPool<T>`: 固定大小内存池，使用空闲链表实现，按需增长
- `ObjPool<T>`: 对象池，使用 placement new 构造对象，通过 `std::integral_constant` 分发处理 trivial/non-trivial 类型

### 2. 上下文层 (`include/context.h`, `include/coroutine.h`)
- `Context`: 封装 `ucontext_t`，管理栈分配和上下文切换
- `Coroutine`: 协程封装，状态包括 READY/RUNNING/SUSPEND/DEAD

### 3. 调度层 (`include/processor.h`, `include/scheduler.h`, `include/processor_selector.h`)
- `Processor`: 单线程调度器，事件循环顺序: timer → pending → epoll → cleanup
- `Scheduler`: 全局单例，管理多个 Processor，自动按 CPU 核心数选择
- `ProcessorSelector`: 调度策略，包括 MIN_EVENT_FIRST (默认) 和 ROUND_ROBIN

### 4. 事件层 (`include/epoller.h`, `include/timer.h`)
- `Epoller`: EPOLLIN/EPOLLOUT 事件注册，与协程映射
- `Timer`: 最小堆存储 (Time, Coroutine*) 对，使用 timerfd 唤醒

### 5. 时间层 (`include/mstime.h`)
- `Time`: 时间封装类，提供当前时间获取、时间比较、时间间隔计算等功能
- 支持毫秒级精度，提供与 `timespec` 结构体的转换

### 6. 同步原语 (`include/mutex.h`, `include/spinlock.h`, `include/spinlock_guard.h`)
- `Spinlock`: 原子比较交换锁
- `SpinlockGuard`: RAII 包装器，独立的头文件
- `RWMutex`: 协程安全的读写锁，带等待队列

### 7. 网络层 (`include/socket.h`, `include/tcp/`)
- `Socket`: 封装 TCP/UDP socket，支持协程化 IO 操作
- `TcpServer`: 基于 Reactor 模式的 TCP 服务器，支持单线程和多线程模式
- `TcpClient`: TCP 客户端封装，提供协程安全的连接、收发接口

### 8. RPC 层 (`include/rpc/`)
- `RpcServer`: RPC 服务器，基于 TcpServer 封装，支持服务注册与请求路由
- `RpcClient`: RPC 客户端，提供同步调用接口
- `RpcHeader`: RPC 协议头部（固定 8 字节），解决 TCP 粘包问题
- `RpcServerStub`: 服务端存根，负责消息编解码
- `RpcClientStub`: 客户端存根，负责消息编解码和网络通信
- `Service`: 服务接口基类，用户需继承实现具体服务
- `TinyJson`: 轻量级 JSON 序列化库（`include/json.h`）

## 公共 API

### 协程调度 API
```cpp
namespace minico {

// 创建并调度新协程
void co_go(std::function<void()> &func, size_t stackSize = parameter::coroutineStackSize, int tid = -1);
void co_go(std::function<void()> &&func, size_t stackSize = parameter::coroutineStackSize, int tid = -1);

// 协程休眠
void co_sleep(Time t);

// 等待调度器停止
void sche_join();

}
```

### RPC 服务端 API
```cpp
class RpcServer {
public:
    // 启动 RPC 服务器（单线程模式）
    void start(std::string_view ip, int port);
    
    // 启动 RPC 服务器（多线程模式）
    void start_multi(std::string_view ip, int port);
    
    // 注册服务
    void add_service(Service *s);
};

// 服务接口（用户继承实现）
class Service {
public:
    virtual const char *name() const = 0;
    virtual void process(TinyJson &request, TinyJson &result) = 0;
};
```

### RPC 客户端 API
```cpp
class RpcClient {
public:
    // 连接服务器
    void connect(const char *ip, int port);
    
    // 发起 RPC 调用
    void call(TinyJson &request, TinyJson &response);
    
    // 心跳检测
    void ping();
    
    // 关闭连接
    int close();
};
```

## 关键设计模式

1. **双缓冲待调度队列**: Processor 中使用两个队列交替，减少生产者写入与消费者读取的锁竞争
2. **对象池复用**: Coroutine 对象通过 ObjPool 分配，避免频繁内存分配
3. **线程本地存储**: `thread_local int threadIdx` 用于处理器识别
4. **RAII 资源管理**: SpinlockGuard、Context 等均采用 RAII 模式
5. **Reactor 模式**: TcpServer 主协程负责 accept，新连接派发独立协程处理
6. **存根模式**: RPC 层通过 ServerStub/ClientStub 封装网络通信细节
7. **服务注册模式**: RpcServer 通过 `add_service()` 动态注册服务，支持多服务路由

## RPC 协议设计

### 消息格式
```
+----------------+----------------+------------------+
| info (2 bytes) | magic (2 bytes)| len (4 bytes)    |
+----------------+----------------+------------------+
|              JSON Payload (len bytes)             |
+---------------------------------------------------+
```

- `info`: 消息类型/版本信息
- `magic`: 魔数 (0x7777)，用于校验合法 RPC 消息
- `len`: JSON 载荷长度（网络字节序）

### 请求格式
```json
{
    "service": "ServiceName",
    "method": "MethodName",
    // 其他业务参数...
}
```

### 响应格式
```json
{
    "err": 200,
    "errmsg": "ok",
    // 其他返回数据...
}
```

## 目录结构

```
rpc-project/
├── include/              # 头文件
│   ├── context.h         # 上下文封装
│   ├── coroutine.h       # 协程定义
│   ├── epoller.h         # epoll 封装
│   ├── json.h            # TinyJson 序列化库
│   ├── logger.h          # 日志系统
│   ├── mempool.h         # 内存池
│   ├── minico_api.h      # 公共 API
│   ├── mstime.h          # 时间封装类
│   ├── mutex.h           # 读写锁
│   ├── objpool.h         # 对象池
│   ├── parameter.h       # 编译期参数
│   ├── processor.h       # 处理器/调度器
│   ├── processor_selector.h  # 处理器选择策略
│   ├── scheduler.h       # 全局调度器
│   ├── socket.h          # 网络封装
│   ├── spinlock.h        # 自旋锁
│   ├── spinlock_guard.h  # 自旋锁 RAII 包装器
│   ├── timer.h           # 定时器
│   ├── utils.h           # 工具宏
│   ├── rpc/              # RPC 模块
│   │   ├── rpc_client.h      # RPC 客户端
│   │   ├── rpc_client_stub.h # RPC 客户端存根
│   │   ├── rpc_header.h      # RPC 协议头部
│   │   ├── rpc_server.h      # RPC 服务器
│   │   ├── rpc_server_stub.h # RPC 服务端存根
│   │   └── service.h         # 服务接口
│   └── tcp/              # TCP 网络模块
│       ├── tcp_client.h  # TCP 客户端
│       └── tcp_server.h  # TCP 服务端
├── src/                  # 源文件实现
│   ├── context.cpp
│   ├── coroutine.cpp
│   ├── epoller.cpp
│   ├── logger.cpp
│   ├── minico_api.cpp
│   ├── mstime.cpp
│   ├── mutex.cpp
│   ├── processor.cpp
│   ├── processor_selector.cpp
│   ├── scheduler.cpp
│   ├── socket.cpp
│   ├── timer.cpp
│   ├── rpc/              # RPC 模块实现
│   │   ├── rpc_client.cpp
│   │   ├── rpc_client_stub.cpp
│   │   ├── rpc_header.cpp
│   │   ├── rpc_server.cpp
│   │   └── rpc_server_stub.cpp
│   └── tcp/
│       ├── tcp_client.cpp
│       └── tcp_server.cpp
├── examples/             # 测试用例
│   ├── processor_smoke_test.cpp  # 核心冒烟测试
│   ├── tcp_client_test.cpp       # TCP 客户端测试
│   ├── tcp_server_test.cpp       # TCP 服务端测试
│   ├── rpc_client.cpp            # RPC 客户端测试
│   ├── rpc_server.cpp            # RPC 服务端测试
│   ├── log_test.cpp              # 日志测试
│   └── timer_epoller_test.cpp    # 定时器测试
├── bin/                  # 可执行文件输出
├── lib/                  # 库文件输出
└── build/                # 构建目录
```

## 已知问题

| 位置 | 问题描述 | 状态 |
|------|----------|------|
| `src/context.cpp:83` | 使用硬编码栈大小 `parameter::coroutineStackSize` 而非构造函数参数 `stackSize_`（有 TODO 注释）| 待修复 |
| `src/mutex.cpp:17` | `rlock()` 递归调用：协程被唤醒后会再次调用 `rlock()` 尝试获取锁 | 设计如此 |

## 编码规范

1. **命名空间**: 所有代码位于 `minico` 命名空间
2. **禁用拷贝/移动**: 使用 `DISALLOW_COPY_MOVE_AND_ASSIGN` 宏
3. **日志宏**: `LOG_INFO`, `LOG_ERROR`, `LOG_DEBUG`
4. **成员变量**: 使用下划线后缀 (`_sockfd`, `_timeVal`) 或下划线前缀 (`_freeListHead`)
5. **注释风格**: 使用 Doxygen 风格文档注释
6. **模板编程**: 使用 `std::forward` 完美转发，`std::integral_constant` 类型分发

## 测试

主要测试文件：
- `examples/processor_smoke_test.cpp`: 验证协程创建、执行、定时器暂停与恢复、处理器事件循环功能
- `examples/tcp_server_test.cpp`: TCP 服务端功能测试
- `examples/tcp_client_test.cpp`: TCP 客户端功能测试
- `examples/rpc_server.cpp`: RPC 服务端功能测试，包含自定义服务实现示例
- `examples/rpc_client.cpp`: RPC 客户端功能测试，演示服务调用流程

## 依赖

- **操作系统**: Linux（依赖 epoll、timerfd、ucontext）
- **编译器**: GCC 或 Clang（需要支持 ucontext API）
- **库**: pthread
- **构建工具**: CMake 3.16+
- **语言标准**: C++17 兼容编译器

## 代码修改注意事项

1. **新增源文件**: 需要在 `src/CMakeLists.txt` 中添加到 SOURCES 列表
2. **新增测试文件**: 需要在 `examples/CMakeLists.txt` 中添加对应的 `add_executable` 和 `target_link_libraries`
3. **RPC 模块**: RPC 相关代码已集成到主库 `librpc-project.so`，无需单独编译
4. **新增 RPC 服务**: 继承 `Service` 类并实现 `name()` 和 `process()` 方法，通过 `RpcServer::add_service()` 注册
5. **协程安全**: 在协程环境中避免使用阻塞式系统调用，应使用框架提供的协程化接口
6. **跨平台**: 代码依赖 Linux 特有 API，无法在 Windows/macOS 上编译运行
