# RPC 协程框架

基于 GNU ucontext 的高性能用户态协程 RPC 框架，C++17 实现。

> 本项目基于 [FlameHize/minico-RPC](https://github.com/FlameHize/minico-RPC) 修改，感谢 UESTC 学长的开源贡献！

## 特性

- 🚀 **高性能**：10K 并发下 310K QPS
- 🔄 **用户态协程**：轻量级上下文切换
- 📡 **异步 RPC**：完整的请求/响应模型
- ⚡ **多核并行**：SO_REUSEPORT + 负载均衡

## 快速开始

### 编译

```bash
mkdir build && cd build
cmake .. && make
```

编译成功后生成以下可执行文件：
- `bin/rpc_server` - RPC 服务端示例
- `bin/rpc_client` - RPC 客户端示例
- `bin/rpc_benchmark` - 性能压测工具

### 运行示例

**启动服务端**（终端 1）：
```bash
./bin/rpc_server
# 输出：[System] Starting RPC Server Node...
```

**调用客户端**（终端 2）：
```bash
./bin/rpc_client
# 输出：[GetStatus] status: 200
#       [GetMetrics] status: 200
```

### 性能测试

**简单压测**（100 并发，5 秒）：
```bash
./bin/rpc_benchmark -c 100 -t 5
```

**C10K 极限压测**（10,000 并发，10 秒）：
```bash
./bin/rpc_benchmark -c 10000 -t 10
# 预期输出：310,000+ QPS, P50 < 1ms
```

### 单元测试（开发者用）

```bash
./bin/processor_smoke_test    # 协程调度器测试
./bin/timer_epoller_test      # 定时器+epoll 测试
./bin/tcp_server_test         # TCP 层测试
```

## 自定义服务

### 服务端示例

```cpp
#include "rpc/rpc_server.h"

class MyService : public Service {
    const char* name() const override { return "MyService"; }
    void process(TinyJson& req, TinyJson& res) override {
        res["result"] = "hello";
    }
};

int main() {
    RpcServer server;
    server.add_service(new MyService());
    server.start_multi("0.0.0.0", 12345);  // 多核模式
    minico::sche_join();
}
```

### 客户端示例

```cpp
#include "rpc/rpc_client.h"

int main() {
    RpcClient client;
    client.connect("127.0.0.1", 12345);
    
    TinyJson req, res;
    req["service"].Set<std::string>("MyService");
    client.call(req, res);
    
    std::cout << res["result"] << std::endl;
}
```

完整示例代码见 `examples/` 目录。

## 目录说明

| 目录 | 说明 |
|------|------|
| `tests/` | 单元测试 |
| `scripts/` | 性能分析工具 |
| `examples/` | 示例程序 |

## 文档

- [AGENTS.md](AGENTS.md) - 完整架构文档
- [scripts/README.md](scripts/README.md) - 性能测试指南

## 性能

**10K 并发压测**：310,825 QPS，P50 延迟 0.19ms

详见 [AGENTS.md#性能测试报告](AGENTS.md#性能测试报告)

## 致谢

本项目基于 [FlameHize/minico-RPC](https://github.com/FlameHize/minico-RPC) 修改，感谢 UESTC 学长的开源贡献！
