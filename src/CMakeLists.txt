add_library(rpc-project SHARED
    logger.cpp
)

target_include_directories(rpc-project PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)