# RPC 项目性能分析工具

本目录提供 RPC 项目的性能分析工具，包括火焰图生成、性能统计、实时监控等功能。

## 目录说明

| 目录/文件 | 说明 | Git |
|-----------|------|-----|
| `*.sh` | 性能分析脚本（入口工具） | ✅ 提交 |
| `flamegraph/` | 火焰图工具（来自 [brendangregg/FlameGraph](https://github.com/brendangregg/FlameGraph)） | ✅ 提交 |
| `output/` | **运行时生成的测试结果** | ❌ 不提交 |

> **注意**: `output/` 目录下的文件由工具运行时自动生成，已配置 `.gitignore` 忽略，请勿提交。

## WSL 安装 perf

参考：https://www.cnblogs.com/Ziyoung/p/19184924

```bash
sudo apt update
sudo apt install linux-tools-generic

# 定位到 /usr/lib/linux-tools 文件夹下
cd /usr/lib/linux-tools

# 进入和内核版本号匹配的文件夹（如 6.8.0-87-generic）
cd 6.8.0-87-generic

# 将 perf 复制到 /usr/local/bin
sudo cp perf /usr/local/bin
```

## 目录结构

```
scripts/
├── README.md              # 本文档
├── generate-flamegraph.sh # 火焰图生成脚本
├── perf-stat.sh           # 性能统计脚本
├── perf-top.sh            # 实时监控脚本
├── perf-report.sh         # 性能报告脚本
├── perf-diff.sh           # 性能对比脚本
├── flamegraph/            # 火焰图工具 (brendangregg/FlameGraph)
│   ├── flamegraph.pl
│   ├── stackcollapse-perf.pl
│   └── stackcollapse.pl
└── output/                # 测试结果输出
    ├── flamegraph.svg     # 火焰图
    └── out.folded         # 折叠的调用栈数据
```

## 脚本说明

| 脚本 | 功能 |
|------|------|
| `generate-flamegraph.sh` | 生成火焰图 SVG |
| `perf-stat.sh` | 采集性能统计数据 |
| `perf-top.sh` | 实时监控性能热点 |
| `perf-report.sh` | 查看性能分析报告 |
| `perf-diff.sh` | 对比两次性能测试 |

## 快速开始

### 1. 生成火焰图

```bash
# 交互式模式
./generate-flamegraph.sh

# 指定程序和采样时长
./generate-flamegraph.sh rpc_server 30
```

### 2. 性能统计

```bash
# 查看 CPU、缓存等统计信息
./perf-stat.sh rpc_server
```

### 3. 实时性能监控

```bash
# 实时监控运行中的进程
./perf-top.sh <pid>
```

### 4. 查看性能报告

```bash
# 查看最新的 perf 报告
./perf-report.sh

# 指定 perf 文件
./perf-report.sh output/rpc_server_20260602_120000.perf
```

### 5. 性能对比

```bash
# 对比优化前后的性能
./perf-diff.sh output/before.perf output/after.perf
```

## 输出文件

所有输出文件保存在 `output/` 目录：

- `*.perf` - perf 原始数据 (测试后建议删除，占用空间大)
- `*.script` - perf script 输出 (测试后建议删除)
- `*.folded` - 折叠的栈数据 (保留，用于分析)
- `*.svg` - 火焰图 (保留，可用浏览器打开)

## 测试结果

### 测试环境

- **测试时间**: 2026-06-06
- **测试程序**: rpc_server
- **采样频率**: 99 Hz
- **调用栈类型**: dwarf (内联函数展开)

### 测试结果概览

| 指标 | 数值 |
|------|------|
| 总采样数 | 243,607,353,563 |
| 采样函数数 | 1,702 |
| 火焰图文件 | output/flamegraph.svg |

### 各模块 CPU 时间占比

| 模块 | 采样数 | 占比 |
|------|--------|------|
| RpcServer::on_connection | 231,922,001,329 | 95.20% |
| swapcontext (协程切换) | 8,479,006,410 | 3.48% |
| RpcServer::process | 28,872,272,336 | 11.85% |
| ServiceDispatcher | 26,804,127,757 | 11.00% |
| epoll_wait | 2,936,948,377 | 1.20% |
| find_service | 502,626,772 | 0.20% |
| accept_raw | 234,160,714 | 0.09% |

### 主要调用路径分析

```
网络 I/O (epoll/socket)        173,289,818,694 采样
JSON 序列化/反序列化 (TinyJson)  32,322,566,271 采样
协程调度 (swapcontext)         236,470,072,221 采样
RPC 处理 (RpcServer)           231,965,125,534 采样
服务查找 (unordered_map)        1,097,847,431 采样
内存分配 (malloc/free)         23,569,250,194 采样
```

### 性能热点 Top 10

| 排名 | 函数 | 采样数 |
|------|------|--------|
| 1 | tcp_ack | 2,483,391,964 |
| 2 | do_epoll_ctl | 2,418,879,520 |
| 3 | tcp_sendmsg_locked | 2,233,399,283 |
| 4 | RpcServer::on_connection | 1,413,996,818 |
| 5 | tcp_v4_rcv | 1,370,604,223 |
| 6 | kmem_cache_free | 1,261,903,026 |
| 7 | __tcp_transmit_skb | 1,196,779,642 |
| 8 | enqueue_to_backlog | 1,132,075,919 |
| 9 | swapcontext | 1,112,570,578 |
| 10 | get_obj_cgroup_from_current | 1,107,633,526 |

### 分析结论

1. **RPC 处理是主要耗时**: `RpcServer::on_connection` 占用 95.20% 的 CPU 时间，这是预期的，因为它是 RPC 服务器的核心处理函数。

2. **协程切换开销**: `swapcontext` 占用 3.48%，说明协程上下文切换有一定开销，但在可接受范围内。

3. **网络 I/O 开销**: epoll 和 socket 操作占比较小，说明网络层效率较高。

4. **服务查找高效**: `find_service` 仅占 0.20%，unordered_map 的 O(1) 查找性能良好。

5. **JSON 序列化**: TinyJson 序列化/反序列化占用约 13% 的 CPU 时间，是主要优化点之一。

## 优化建议

1. **减少协程切换**: 考虑优化协程调度策略，减少不必要的上下文切换。

2. **优化 JSON 序列化**: 
   - 使用字符串池减少内存分配
   - 预分配缓冲区避免反复扩容

3. **连接池优化**: 复用 TCP 连接，减少 accept 和 epoll_ctl 调用。

4. **零拷贝优化**: 考虑使用 sendfile 或 io_uring 减少数据拷贝。

## 火焰图解读

- **红色区域** - CPU 时间占比高的函数
- **宽度** - 函数执行时间占比
- **高度** - 调用栈深度
- **从左到右** - 调用关系

## 常用分析场景

### 查找性能瓶颈

```bash
# 1. 运行服务器
./bin/rpc_server &

# 2. 运行负载测试
./bin/rpc_benchmark &

# 3. 采集性能数据
./generate-flamegraph.sh rpc_server 30

# 4. 查看火焰图
# 浏览器打开 output/*.svg
```

### 对比优化效果

```bash
# 优化前
./generate-flamegraph.sh rpc_server 30
mv output/*.perf output/before.perf

# 进行优化...

# 优化后
./generate-flamegraph.sh rpc_server 30
mv output/*.perf output/after.perf

# 对比
./perf-diff.sh output/before.perf output/after.perf
```

## 注意事项

1. perf 数据文件 (*.perf) 通常很大 (数十 MB 到数百 MB)，测试后建议删除。
2. 火焰图生成需要 root 权限或无 cap_setuid 能力的 perf。
3. 建议在 `.gitignore` 中添加 `scripts/output/*.perf` 避免提交大文件。
