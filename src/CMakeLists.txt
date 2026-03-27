file(GLOB SOURCES "*.cpp" )

add_library(rpc-project SHARED ${SOURCES})

target_include_directories(rpc-project PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(rpc-project PUBLIC Threads::Threads)