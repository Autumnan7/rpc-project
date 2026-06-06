#!/bin/bash
# 查看 perf 报告脚本
# 用法：./perf-report.sh [perf_data_file]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/output"

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Perf 性能分析报告${NC}"
echo -e "${GREEN}========================================${NC}"

# 查找最新的 perf 数据文件
if [ $# -eq 0 ]; then
    LATEST_PERF=$(ls -t "$OUTPUT_DIR"/*.perf 2>/dev/null | head -1)
    if [ -z "$LATEST_PERF" ]; then
        echo -e "${RED}错误：未找到 perf 数据文件${NC}"
        echo -e "${YELLOW}请先运行 generate-flamegraph.sh 采集数据${NC}"
        exit 1
    fi
    echo -e "${YELLOW}使用最新的 perf 数据：$LATEST_PERF${NC}"
    PERF_FILE=$LATEST_PERF
else
    PERF_FILE=$1
    if [ ! -f "$PERF_FILE" ]; then
        echo -e "${RED}错误：文件不存在 $PERF_FILE${NC}"
        exit 1
    fi
fi

echo ""
echo -e "${YELLOW}生成报告...${NC}"
echo ""

# 生成文本报告
perf report -i "$PERF_FILE" --stdio --sort comm,dso,sym
