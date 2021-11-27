cmake_minimum_required(VERSION 3.0)
project(HttpServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS -pthread)

add_executable(HttpServer main.cpp http_conn.cpp)
