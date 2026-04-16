cmake_minimum_required(VERSION 3.10)
project(MyHttpServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找 Boost（muduo 可能需要）
find_package(Boost REQUIRED COMPONENTS system thread)

# 收集源文件（注意路径前缀 src/）
set(SOURCES
    main.cc
    src/http_server.cc
    src/http_request.cc
    src/http_response.cc
    src/http_context.cc
    src/radix_tree.cc
    src/middleware.cc
    src/static_file_handler.cc   # 如果存在，否则删除
    src/utils.cc                 # 如果存在，否则删除
)

add_executable(my_http ${SOURCES})

# 包含头文件目录：根目录（main.cc 可能需要）和 src 目录
target_include_directories(my_http PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接库（muduo 和 Boost、pthread）
target_link_libraries(my_http
    muduo_net
    muduo_base
    ${Boost_LIBRARIES}
    pthread
)