# ================= RPC 服务端示例 =================
add_executable(rpc_server rpc_server.cpp)
target_link_libraries(rpc_server PRIVATE rpc-project)

# ================= RPC 客户端示例 =================
add_executable(rpc_client rpc_client.cpp)
target_link_libraries(rpc_client PRIVATE rpc-project)

# ================= RPC 性能基准测试 =================
add_executable(rpc_benchmark rpc_benchmark.cpp)
target_link_libraries(rpc_benchmark PRIVATE rpc-project)
