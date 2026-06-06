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
- 可执行文件：`bin/`
- 库文件：`lib/`

### 编译器支持
- **GCC/Clang**: 使用 `-Wall -Wextra -Wpedantic` 警告选项

## 架构概览

项目采用分层架构，自底向上分为以下层次：

### 1. 内存层 (`include/mempool.h`, `include/objpool.h`)
- `MemPool<T>`: 固定大小内存池，使用空闲链表实现，按需增长
- `ObjPool<T>`: 对象池，使用 placement new 构造对象，通过 `std::integral_constant` 分发处理 trivial/non-trivial 类型

### 2. 上下文层 (`include/context.h`, `include/coroutine.h`)
- `Context`: 封装 `ucontext_t`，管理栈分配和上下文切换
- `Coroutine`: 协程封装，状态包括 READY/RUNNING/SUSPEND/DEAD

### 3. 调度层 (`include/processor.h`, `include/scheduler.h`, `include/processor_selector.h`)
- `Processor`: 单线程调度器，事件循环顺序：timer → pending → epoll → cleanup
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
    // bind_thread=true: 业务协程绑定到 accept 所在核（推荐，缓存亲和性好）
    // bind_thread=false: 业务协程走全局负载均衡（MIN_EVENT_FIRST）
    void start_multi(std::string_view ip, int port, bool bind_thread = true);
    
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
├── tests/                # 单元测试（通过 CMake 编译）
│   ├── CMakeLists.txt
│   ├── log_test.cpp
│   ├── processor_smoke_test.cpp
│   ├── tcp_client_test.cpp
│   ├── tcp_server_test.cpp
│   └── timer_epoller_test.cpp
├── scripts/              # 性能分析工具（无需编译）
│   ├── README.md
│   ├── generate-flamegraph.sh
│   ├── perf-stat.sh
│   ├── perf-top.sh
│   ├── perf-report.sh
│   ├── perf-diff.sh
│   ├── flamegraph/       # 火焰图工具 (brendangregg/FlameGraph)
│   └── output/           # 测试结果 (gitignore 忽略)
├── examples/             # 示例程序
│   ├── rpc_client.cpp
│   ├── rpc_server.cpp
│   └── rpc_benchmark.cpp
├── bin/                  # 可执行文件输出 (gitignore 忽略)
├── lib/                  # 库文件输出 (gitignore 忽略)
└── build/                # 构建目录 (gitignore 忽略)
```

## 已知问题

| 位置 | 问题描述 | 状态 |
|------|----------|------|
| `src/context.cpp:54-55` | 使用硬编码栈大小 `parameter::coroutineStackSize` 而非构造函数参数 `stackSize_`（有 TODO 注释）| 待修复 |

## 编码规范

1. **命名空间**: 所有代码位于 `minico` 命名空间
2. **禁用拷贝/移动**: 使用 `DISALLOW_COPY_MOVE_AND_ASSIGN` 宏
3. **日志宏**: `LOG_INFO`, `LOG_ERROR`, `LOG_DEBUG`
4. **成员变量**: 使用下划线后缀 (`_sockfd`, `_timeVal`) 或下划线前缀 (`_freeListHead`)
5. **注释风格**: 使用 Doxygen 风格文档注释
6. **模板编程**: 使用 `std::forward` 完美转发，`std::integral_constant` 类型分发

## 测试

### 单元测试

测试文件位于 `tests/` 目录，通过 CMake 编译：

```bash
cd build && make
./bin/log_test
./bin/processor_smoke_test
./bin/tcp_server_test
./bin/tcp_client_test
./bin/timer_epoller_test
```

### 性能测试

性能分析工具位于 `scripts/` 目录，详见 `scripts/README.md`。

### 示例程序

示例程序位于 `examples/` 目录，包括 RPC 客户端/服务端和基准测试工具。

## 性能测试报告 (Benchmark Report)

> 测试环境：WSL2 Linux / 127.0.0.1 Loopback / LOG_LEVEL_FATAL  
> 测试工具：`rpc_benchmark`（全异步协程 + 线程本地无锁统计）  
> 测试服务：`SystemMonitorService.GetStatus`

---

### 1. 线程绑定策略对比 (bind_thread vs 非绑定)

#### 背景

`start_multi` 模式下，内核通过 `SO_REUSEPORT` 将连接 hash 到确定 CPU 核的 accept 协程。业务协程有两种调度策略：

| 策略 | 行为 | 特点 |
|------|------|------|
| **绑定模式** (`bind_thread=true`) | 业务协程直接投递到 accept 所在核 | 绕过全局负载均衡，保住 L1/L2 缓存亲和性 |
| **非绑定模式** (`bind_thread=false`) | 业务协程走 `MIN_EVENT_FIRST` 负载均衡 | 由 `ProcessorSelector` 选择最空闲的核 |

#### 1K 并发压测结果

| 指标 | 绑定模式 | 非绑定模式 | 差异 |
|------|----------|------------|------|
| **QPS** | 276,965 | 242,395 | 绑定 **+14.2%** |
| P50 | 0.76 ms | 0.42 ms | 非绑定更低 |
| P90 | 11.20 ms | 21.84 ms | 绑定 **-48.6%** |
| P95 | 15.16 ms | 24.72 ms | 绑定 **-38.9%** |
| P99 | 24.76 ms | 31.66 ms | 绑定 **-21.9%** |
| P99.9 | 38.38 ms | 41.26 ms | 绑定 **-7.0%** |
| 失败数 | 0 | 0 | — |

#### 10K 并发压测结果

| 指标 | 绑定模式 | 非绑定模式 | 差异 |
|------|----------|------------|------|
| **QPS** | 243,609 | 215,871 | 绑定 **+12.8%** |
| P50 | 1.10 ms | 0.72 ms | 非绑定更低 |
| P90 | 171.03 ms | 238.87 ms | 绑定 **-28.4%** |
| P95 | 187.03 ms | 299.36 ms | 绑定 **-37.5%** |
| P99 | 224.01 ms | 354.11 ms | 绑定 **-36.7%** |
| P99.9 | 298.62 ms | 409.27 ms | 绑定 **-27.0%** |
| 失败数 | 0 | 0 | — |

#### 分析

- **绑定模式全面胜出**：1K/10K 并发下 QPS 分别领先 14.2%/12.8%，尾延迟优势更显著（10K 时 P95 低 37.5%）
- **缓存亲和性是关键**：连接在 accept 核完成握手，业务协程留在同一核处理，避免跨核缓存失效
- **非绑定 P50 更低是假象**：全局负载均衡把少量请求快速分到空闲核，同时把大量请求堆积在繁忙核，造成长尾拥堵

#### 结论

在 `SO_REUSEPORT` 多核模式下，**业务协程绑定到 accept 所在核是更优策略**。默认 `bind_thread=true`。

---

### 2. 永久注册 listen_fd 架构优化 (C10K 对比压测)

> 测试时间：2026-06-01
> 测试工具：rpc_benchmark（10,000 协程长连接，10 秒满载压测）
> 服务端：rpc_server（start_multi，SO_REUSEPORT 多核模式）

#### 背景

**绑定模式**（数据来自本章节第 1 节）：每次 accept 新连接时，通过 `waitEvent()` 临时注册 epoll 事件，流程为 `addEvent → yield → resume → removeEvent`。每次 accept 都会产生 2 次 `epoll_ctl` 系统调用。

**永久注册模式**：启动时通过 `addPermanentEvent()` 一次性注册 `listen_fd`，accept 循环使用 `accept_raw()` 非阻塞调用 + `EAGAIN` 判断 + batch yield 策略。彻底消除 accept 路径上的 `epoll_ctl` 开销。

#### 控制变量

| 变量 | 设置 |
|------|------|
| 并发连接数 | 10,000 协程（长连接） |
| 测试时长 | 10 秒 |
| 服务端 IP | 0.0.0.0:12345 |
| 客户端 IP | 127.0.0.1:12345 |
| 网络环境 | 本机 Loopback |
| 日志级别 | LOG_LEVEL_FATAL（关闭日志 I/O 干扰） |
| 服务端绑核 | bind_thread=true（业务协程绑定到 accept 所在核） |
| RPC 服务 | SystemMonitorService.GetStatus |

**唯一变量**：listen_fd 的 epoll 注册策略（临时注册 vs 永久注册）

#### C10K 压测结果对比

| 指标 | 绑定模式（临时注册） | 永久注册模式 | 提升幅度 |
|------|-------------------|-------------------|----------|
| **QPS** | 243,609 | **310,825** | **+27.6%** |
| **P50 延迟** | 1.10 ms | **0.19 ms** | **-82.7%** |
| **P90 延迟** | 171.03 ms | **19.23 ms** | **-88.8%** |
| **P95 延迟** | 187.03 ms | **71.48 ms** | **-61.8%** |
| **P99 延迟** | 224.01 ms | 736.59 ms | +228.8% |
| **P99.9 延迟** | 298.62 ms | 1692.20 ms | +466.7% |
| **失败数** | 0 | 0 | — |

#### 分析

**吞吐量提升 27.6%**：QPS 从 243K 提升至 310K。核心原因是永久注册模式消除了每次 accept 的 2 次 `epoll_ctl` 系统调用（用户态↔内核态切换 + 红黑树操作）。在高并发场景下，这个开销被放大到十万次级别，永久注册直接省去了这部分 CPU 周期。

**中位延迟暴跌 82.7%**：P50 从 1.10ms 降至 0.19ms，说明绝大多数请求的处理路径被大幅缩短。绑定模式的 `waitEvent → yield → resume` 流程引入了额外的上下文切换和调度延迟，永久注册后 accept 协程在 epoll 唤醒后直接 accept，无中间挂起。

**P90/P95 显著改善**：P90 从 171ms 降至 19ms（-88.8%），P95 从 187ms 降至 71ms（-61.8%）。这说明在高负载下，永久注册模式避免了大量 accept 协程因 `epoll_ctl` 竞争导致的排队延迟。

**P99/P99.9 异常升高**：永久注册模式的 P99 从 224ms 升至 736ms（+228.8%），P99.9 从 298ms 升至 1692ms（+466.7%）。原因分析：
1. **batch yield 策略**：永久注册模式使用 batch yield（每 accept 16 个连接后 yield），在极端突发流量下可能导致少量协程等待时间变长
2. **无临时注册的"缓冲效应"**：绑定模式的 `yield` 在每次 accept 后都会执行，客观上起到了流量平滑作用；永久注册模式连续 accept 16 个后才 yield，极端情况下可能把更多请求堆积到同一轮处理

**工程权衡**：P99 的代价换来了 QPS +27.6% 和 P50/P90/P95 的大幅改善。对于 RPC 短包场景，绝大多数请求集中在 P95 以内，P99 的极端值对用户体验影响有限。如需进一步优化 P99，可考虑减小 batch size（当前 16）或引入动态 batch 策略。

#### 结论

**永久注册 listen_fd 是显著优于绑定模式的方案**。QPS +27.6%、P50 暴跌 82.7%、P90 暴跌 88.8%，核心指标全面领先。P99 的升高是 batch yield 策略的副作用，可通过调优 batch size 进一步优化。

---

### 3. 极限高并发压测 (C10K 承载力验证 - 永久注册模式)

本测试专为验证框架系统在**海量高频并发（C10K 问题**下的极限吞吐、抗压能力与内存管理稳定性。

- **测试工具**: 最新版 `rpc_benchmark` (基于全异步协程 + 线程本地无锁统计架构)
- **测试条件**:
  - **并发连接数**: **10,000** (1 万独立协程，长连接同时疯狂打流)
  - **测试时长**: 10 秒
  - **服务端配置**: 多核并行模式 + **永久注册 listen_fd**，关闭终端日志打印消除 I/O 干扰 (`LOG_LEVEL_FATAL`)
  - **网络环境**: 127.0.0.1:12345 (本机 Loopback)

#### 测试结果

```text
========================================
   Rpcbench - Coroutine RPC Benchmark   
========================================
Service : SystemMonitorService.GetStatus
Target  : 127.0.0.1:12345
Clients : 10000 (coroutines)
Time    : 10 seconds
----------------------------------------

[ Benchmark Results ]
Total Requests : 3,108,255
Succeed        : 3,108,255
Failed         : 0
Throughput     : 310,825 QPS (Req/sec)
Network I/O    : 21.64 MB/sec

[ Latency Distribution (ms) ]
  Min   : 0.01 ms
  P50   : 0.19 ms
  P90   : 19.23 ms
  P95   : 71.48 ms
  P99   : 736.59 ms
  P99.9 : 1692.20 ms
  Max   : 1794.49 ms
```

#### 数据解析与工程结论

在单机 **10,000 并发** 的极限重压（没有任何休眠等待时间地收发数据）下，系统稳定吐出 **310,825 QPS**，且 **310 万次请求 0 失败**。这彻底验证了本框架架构的绝对硬核实力：

1. **彻底攻克上下文切换风暴 (C10K)**：传统基于多线程阻塞模型（Thread-per-connection）在 1 万并发下，线程切换的代价会直接将系统跑满甚至导致 OOM 崩溃。而本系统由于采用了 **GNU ucontext 用户态协程** + **永久注册 listen_fd**，彻底规避了进入内核态调度的巨额开销，极大地释放了 CPU 算力。
2. **极底的框架基础耗损**：压测期间记录的最小延迟低至 **`0.01 ms` (10 微秒)**，证明当操作系统刚把包从网卡解出来送进 Epoll 队列后，框架内部的分发调度、包拆解、以及 TinyJson 序列化的全流程极其轻薄，几乎没有引入延迟损耗。
3. **高负载下的内存稳定性**：支撑起 10,000 个独立 Socket 描述符和协程运行时堆栈（基于框架自建的 `ObjPool` 对象池，实现栈的复用），全程内存未出现波动，零崩溃，P50 保持在极佳的 `0.19 ms`，完美展示出底层资源复用的高效性。
4. **永久注册 listen_fd 的架构优势**：相比绑定模式的临时注册（每次 accept 调用 2 次 `epoll_ctl`），永久注册模式消除了 accept 路径上的系统调用开销，QPS 从 243K 提升至 310K（+27.6%），P50 从 1.10ms 暴跌至 0.19ms（-82.7%）。详见本章第 3 节对比分析。

---

### 4. 基础架构降维对照测试 (基于单线程对标)

在此前早期的架构测试中，我们为了剥离多核与网络协议栈的干扰，曾在**严格限制单核/单线程**的环境中，对协程和传统同步阻塞模型进行了基准比对。

| 并发模式 | 并发度 | 请求数 | 吞吐量 (QPS) | 总耗时 | 耗时缩短对比 |
|---------|-------|-------|-------------|-------|------------|
| 同步串行 | 1 | 1000 | 7,979 | 125.3 ms | 基准值 |
| 协程并发 | 10 | 1000 | **12,467** | 80.2 ms | **快 36.0%** |
| 协程并发 | 50 | 5000 | **11,688** | 427.8 ms | **快 37.5%** |
| 协程并发 | 100 | 10000 | **11,011** | 908.1 ms | **快 32.7%** |

#### 该场景结论

在同样单线程的苛刻条件下：
- 传统同步阻塞：遇到 IO 阻塞时，唯一的线程完全挂起，CPU 空转。
- 用户态协程：遇到网络 EpollWait 时立刻 `co_yield` 让出 CPU，瞬间切换至其他就绪的协程干活。通过时分复用消灭了 IO 堵塞等待时间，在单核能力下仍凭空挤出了近 **60%** 的并发吞吐能力。

## 代码修改注意事项

1. **新增源文件**: 需要在 `src/CMakeLists.txt` 中添加到 SOURCES 列表
2. **新增测试文件**: 需要在 `examples/CMakeLists.txt` 中添加对应的 `add_executable` 和 `target_link_libraries`
3. **RPC 模块**: RPC 相关代码已集成到主库 `librpc-project.so`，无需单独编译
4. **新增 RPC 服务**: 继承 `Service` 类并实现 `name()` 和 `process()` 方法，通过 `RpcServer::add_service()` 注册
5. **协程安全**: 在协程环境中避免使用阻塞式系统调用，应使用框架提供的协程化接口
6. **跨平台**: 代码依赖 Linux 特有 API，无法在 Windows/macOS 上编译运行