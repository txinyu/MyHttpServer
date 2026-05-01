cmake_minimum_required(VERSION 3.10)
project(MyHttpServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# =========================
# 输出目录
# =========================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =========================
# 主服务器程序
# =========================

set(SERVER_SOURCES
    main.cc
    src/http_server.cc
    src/http_request.cc
    src/http_response.cc
    src/http_context.cc
    src/radix_tree.cc
    src/middleware.cc
    src/static_file_handler.cc
    src/utils.cc
)

add_executable(my_http ${SERVER_SOURCES})

target_include_directories(my_http PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(my_http
    muduo_net
    muduo_base
    ${Boost_LIBRARIES}
    Threads::Threads
)

# =========================
# bench 压测程序
# =========================

add_executable(bench_basic
    bench/bench.cpp
)

set_target_properties(bench_basic PROPERTIES
    OUTPUT_NAME bench
)

target_link_libraries(bench_basic
    Threads::Threads
)

add_executable(bench_keepalive
    bench/bench_keepalive.cpp
)

target_link_libraries(bench_keepalive
    Threads::Threads
)

add_executable(latency_bench
    bench/latency_bench.cpp
)

target_link_libraries(latency_bench
    Threads::Threads
)