#!/bin/bash
# 性能对比脚本
# 用法：./perf-diff.sh <base.perf> <new.perf>

set -e

if [ $# -ne 2 ]; then
    echo "用法：$0 <优化前的 perf 文件> <优化后的 perf 文件>"
    echo ""
    echo "示例:"
    echo "  $0 output/before.perf output/after.perf"
    exit 1
fi

BASE_PERF=$1
NEW_PERF=$2

if [ ! -f "$BASE_PERF" ]; then
    echo "错误：找不到基准文件 $BASE_PERF"
    exit 1
fi

if [ ! -f "$NEW_PERF" ]; then
    echo "错误：找不到对比文件 $NEW_PERF"
    exit 1
fi

echo "性能对比分析"
echo "============"
echo "基准：$BASE_PERF"
echo "对比：$NEW_PERF"
echo ""

perf diff --base "$BASE_PERF" --no-inline
