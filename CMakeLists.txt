cmake_minimum_required(VERSION 3.20)
project(MyWebServer)

set(CMAKE_CXX_STANDARD 14)

add_executable(WebServer main.cpp threadpool.h http_conn.cpp http_conn.h noactive/*.cpp))
