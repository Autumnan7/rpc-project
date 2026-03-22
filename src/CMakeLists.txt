add_library(rpc-project SHARED
    logger.cpp
    epoller.cpp
    timer.cpp
    mstime.cpp
)

target_include_directories(rpc-project PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)