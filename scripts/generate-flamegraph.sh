#!/bin/bash
# 火焰图分析脚本
# 用法：./generate-flamegraph.sh [server|client|benchmark] [duration]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BIN_DIR="$PROJECT_DIR/bin"
OUTPUT_DIR="$SCRIPT_DIR/output"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}RPC 项目火焰图分析工具${NC}"
echo -e "${GREEN}========================================${NC}"

# 检查 flamegraph 工具是否安装
check_flamegraph_tools() {
    if [ ! -d "$SCRIPT_DIR/flamegraph" ]; then
        echo -e "${YELLOW}[提示] 未找到 flamegraph 工具，正在克隆...${NC}"
        git clone --depth 1 https://github.com/brendangregg/FlameGraph.git "$SCRIPT_DIR/flamegraph"
        echo -e "${GREEN}[成功] flamegraph 工具已安装${NC}"
    fi
}

# 生成火焰图
generate_flamegraph() {
    local perf_data=$1
    local target=$2
    local duration=$3

    echo -e "${GREEN}[1/4] 开始性能采样...${NC}"

    # 使用 perf 进行采样
    perf record -F 99 -g --call-graph dwarf -o "$OUTPUT_DIR/${target}_${TIMESTAMP}.perf" \
        "$BIN_DIR/$target" &
    local perf_pid=$!

    echo -e "${YELLOW}[等待] 运行 $duration 秒后进行采样...${NC}"
    sleep $duration

    # 停止 perf 采样
    echo -e "${GREEN}[2/4] 停止采样，处理数据...${NC}"
    kill $perf_pid 2>/dev/null || true
    wait $perf_pid 2>/dev/null || true

    # 生成 perf script
    echo -e "${GREEN}[3/4] 生成 perf script...${NC}"
    perf script -i "$OUTPUT_DIR/${target}_${TIMESTAMP}.perf" > "$OUTPUT_DIR/${target}_${TIMESTAMP}.script"

    # 生成火焰图
    echo -e "${GREEN}[4/4] 生成火焰图 SVG...${NC}"
    "$SCRIPT_DIR/flamegraph/stackcollapse-perf.pl" "$OUTPUT_DIR/${target}_${TIMESTAMP}.script" > "$OUTPUT_DIR/${target}_${TIMESTAMP}.folded"
    "$SCRIPT_DIR/flamegraph/flamegraph.pl" "$OUTPUT_DIR/${target}_${TIMESTAMP}.folded" > "$OUTPUT_DIR/${target}_${TIMESTAMP}.svg"

    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}完成！火焰图已生成:${NC}"
    echo -e "  ${OUTPUT_DIR}/${target}_${TIMESTAMP}.svg"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${YELLOW}提示：使用浏览器打开 SVG 文件查看火焰图${NC}"
}

# 交互式模式
interactive_mode() {
    echo -e "${YELLOW}可用的目标程序:${NC}"
    echo "1) rpc_server  - RPC 服务器"
    echo "2) rpc_client  - RPC 客户端"
    echo "3) rpc_benchmark - 性能测试"
    echo "4) rpc_demo    - 演示程序"
    echo "5) tcp_server_test - TCP 服务器测试"
    echo "6) tcp_client_test - TCP 客户端测试"

    echo ""
    read -p "请选择目标程序 (1-6): " choice

    case $choice in
        1) TARGET="rpc_server" ;;
        2) TARGET="rpc_client" ;;
        3) TARGET="rpc_benchmark" ;;
        4) TARGET="rpc_demo" ;;
        5) TARGET="tcp_server_test" ;;
        6) TARGET="tcp_client_test" ;;
        *) echo "无效选择"; exit 1 ;;
    esac

    read -p "请输入采样时长 (秒，默认 30): " DURATION
    DURATION=${DURATION:-30}

    generate_flamegraph "" "$TARGET" "$DURATION"
}

# 主逻辑
check_flamegraph_tools

if [ $# -eq 0 ]; then
    interactive_mode
else
    TARGET=$1
    DURATION=${2:-30}

    if [ ! -f "$BIN_DIR/$TARGET" ]; then
        echo -e "${RED}错误：找不到目标程序 $TARGET${NC}"
        echo -e "${YELLOW}可用的程序:${NC}"
        ls -1 "$BIN_DIR"
        exit 1
    fi

    generate_flamegraph "" "$TARGET" "$DURATION"
fi
