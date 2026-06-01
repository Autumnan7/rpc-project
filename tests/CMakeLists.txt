# ================= 日志模块测试 =================
add_executable(log_test log_test.cpp)
target_link_libraries(log_test PRIVATE rpc-project)

# ================= 处理器冒烟测试 =================
add_executable(processor_smoke_test processor_smoke_test.cpp)
target_link_libraries(processor_smoke_test PRIVATE rpc-project)

# ================= epoll + 定时器测试 =================
add_executable(timer_epoller_test timer_epoller_test.cpp)
target_link_libraries(timer_epoller_test PRIVATE rpc-project)

# ================= TCP 服务端测试 =================
add_executable(tcp_server_test tcp_server_test.cpp)
target_link_libraries(tcp_server_test PRIVATE rpc-project)

# ================= TCP 客户端测试 =================
add_executable(tcp_client_test tcp_client_test.cpp)
target_link_libraries(tcp_client_test PRIVATE rpc-project)
