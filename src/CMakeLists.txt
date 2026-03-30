file(GLOB SOURCES "*.cpp")

add_library(rpc-project SHARED ${SOURCES}
        ../include/json.h
        ../include/rpc/rpc_header.h
        ../include/rpc/service.h
        rpc/rpc_header.cpp
        ../include/rpc/rpc_server_stub.h
        rpc/rpc_server_stub.cpp
        ../include/rpc/rpc_server.h
        rpc/rpc_server.cpp
        ../include/rpc/rpc_client_stub.h
        ../include/rpc/rpc_client.h
        rpc/rpc_client.cpp
        rpc/rpc_client_stub.cpp)

target_include_directories(rpc-project PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(rpc-project PUBLIC Threads::Threads)