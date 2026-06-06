# RPC 协程框架

基于 GNU ucontext 的高性能用户态协程 RPC 框架，C++17 实现。

> 本项目基于 [FlameHize/minico-RPC](https://github.com/FlameHize/minico-RPC) 修改，感谢 UESTC 学长的开源贡献！

## 特性

- 🚀 **高性能**：10K 并发下 310K QPS
- 🔄 **用户态协程**：轻量级上下文切换
- 📡 **异步 RPC**：完整的请求/响应模型
- ⚡ **多核并行**：SO_REUSEPORT + 负载均衡

## 快速开始

```bash
# 编译
mkdir build && cd build
cmake .. && make

# 冒烟测试
./bin/processor_smoke_test
```

## 示例

### 服务端

```cpp
#include "rpc_server.h"

class MyService : public Service {
    const char* name() const override { return "MyService"; }
    void process(TinyJson& req, TinyJson& res) override {
        res["result"] = "hello";
    }
};

int main() {
    RpcServer server;
    server.add_service(new MyService());
    server.start("0.0.0.0", 12345);
}
```

### 客户端

```cpp
#include "rpc_client.h"

int main() {
    RpcClient client;
    client.connect("127.0.0.1", 12345);
    
    TinyJson req, res;
    req["service"] = "MyService";
    client.call(req, res);
    
    std::cout << res["result"] << std::endl;
}
```

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
