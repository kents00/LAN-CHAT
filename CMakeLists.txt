cmake_minimum_required(VERSION 3.16)
project(LAN_Chat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
set(SOURCES
    src/main.cpp
    src/socket_wrapper.cpp
    src/server.cpp
    src/client.cpp
    src/network_manager.cpp
    src/chat_session.cpp
    src/client_handler.cpp
    src/room.cpp
)

add_executable(LAN_Chat ${SOURCES})

# Include headers
target_include_directories(LAN_Chat PRIVATE include)

# Windows: link Winsock2
if(WIN32)
    target_link_libraries(LAN_Chat PRIVATE ws2_32)
    # Enable ANSI escape codes on Windows 10+
    target_compile_definitions(LAN_Chat PRIVATE _WIN32_WINNT=0x0600)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(LAN_Chat PRIVATE /W4)
else()
    target_compile_options(LAN_Chat PRIVATE -Wall -Wextra -Wpedantic)
endif()
