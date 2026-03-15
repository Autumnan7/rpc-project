add_executable(minico_rpc_demo
    main.cpp
)

target_include_directories(minico_rpc_demo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)