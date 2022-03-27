cmake_minimum_required(VERSION 3.0)
project(websocket_demo)

set(CMAKE_CXX_STANDARD 17)

add_executable(websocket_demo
        main.cpp)

target_link_libraries(websocket_demo
        uSockets
        z
        ssl
        crypto
        pthread)