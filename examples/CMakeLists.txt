# ================= TCP 服务端测试 =================
add_executable(tcp_server_test tcp_server_test.cpp)
target_link_libraries(tcp_server_test PRIVATE rpc-project)

# ================= TCP 客户端测试 =================
add_executable(tcp_client_test tcp_client_test.cpp)
target_link_libraries(tcp_client_test PRIVATE rpc-project)

# ================= RPC 服务端测试 =================
add_executable(rpc_server rpc_server.cpp)
target_link_libraries(rpc_server PRIVATE rpc-project)

# ================= RPC 客户端测试 =================
add_executable(rpc_client rpc_client.cpp)
target_link_libraries(rpc_client PRIVATE rpc-project)
