cmake_minimum_required(VERSION 3.10)
project(MyHttpServer)

# 这里改成 17！！！
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS system thread)

add_executable(my_http
    main.cc
    http_server.cc
    http_request.cc
    http_response.cc
    http_context.cc
    radix_tree.cc
    middleware.cc
    static_file_handler.cc
    utils.cc
)

target_link_libraries(my_http
    muduo_net
    muduo_base
    ${Boost_LIBRARIES}
    pthread
)