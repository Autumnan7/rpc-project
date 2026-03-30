# ================= TCP 服务端测试 =================
# 1. 定义目标名称为 tcp_server_test
add_executable(tcp_server_test
        tcp_server_test.cpp
        ../src/tcp/tcp_server.cpp
)

target_link_libraries(tcp_server_test PRIVATE rpc-project)


# ================= TCP 客户端测试 =================
# 1. 定义目标名称为 tcp_client_test
add_executable(tcp_client_test
        tcp_client_test.cpp
        ../src/tcp/tcp_client.cpp
        rpc_client.cpp
        rpc_server.cpp
)

target_link_libraries(tcp_client_test PRIVATE rpc-project)