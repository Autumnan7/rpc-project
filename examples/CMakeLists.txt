add_executable(rpc_demo
    # log_test.cpp
    # test_timer_epoller.cpp
    test.cpp
)

target_link_libraries(rpc_demo PRIVATE rpc-project)