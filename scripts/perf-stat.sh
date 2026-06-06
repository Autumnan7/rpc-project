#!/bin/bash
# 性能统计脚本
# 用法：./perf-stat.sh [server|client|benchmark]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BIN_DIR="$PROJECT_DIR/bin"

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}RPC 项目性能统计${NC}"
echo -e "${GREEN}========================================${NC}"

if [ $# -eq 0 ]; then
    echo -e "${YELLOW}用法：$0 [程序名]${NC}"
    echo -e "${YELLOW}可用的程序:${NC}"
    ls -1 "$BIN_DIR"
    exit 1
fi

TARGET=$1

if [ ! -f "$BIN_DIR/$TARGET" ]; then
    echo -e "${RED}错误：找不到目标程序 $TARGET${NC}"
    exit 1
fi

echo -e "${YELLOW}运行性能统计...${NC}"
echo ""

# 采集详细的 CPU 和缓存统计
perf stat -e \
    cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses, \
    context-switches,cpu-migrations,page-faults, \
    sched:sched_switch \
    "$BIN_DIR/$TARGET" "$@"
