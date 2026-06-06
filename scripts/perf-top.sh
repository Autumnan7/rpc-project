#!/bin/bash
# 实时性能监控脚本
# 用法：./perf-top.sh <pid>

set -e

if [ $# -eq 0 ]; then
    echo "用法：$0 <pid>"
    echo "示例：$0 $(pgrep -f rpc_server | head -1)"
    exit 1
fi

PID=$1

if ! kill -0 $PID 2>/dev/null; then
    echo "错误：进程 $PID 不存在"
    exit 1
fi

echo "实时监控进程 $PID 的性能热点..."
echo "按 'q' 键退出"
echo ""

# 实时显示性能热点
perf top -p $PID -F 99 --sort comm,sym
