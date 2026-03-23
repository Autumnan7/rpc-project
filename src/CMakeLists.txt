add_library(rpc-project SHARED
    logger.cpp
    epoller.cpp
    timer.cpp
    mstime.cpp
    context.cpp
    coroutine.cpp
    processor.cpp
)

target_include_directories(rpc-project PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(rpc-project PUBLIC Threads::Threads)