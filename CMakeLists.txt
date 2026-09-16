cmake_minimum_required(VERSION 4.4)
project(HTTPServer)

set(CMAKE_CXX_STANDARD 20)

add_executable(HTTPServer main.cpp)
add_executable(HTTPClient client.cpp)
