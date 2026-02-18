@echo off
REM build.bat - Quick build script for LAN Chat v2.0 (MinGW g++ 6.3.0+)
echo [Build] Compiling LAN Chat v2.0 (multi-PC)...
if not exist build mkdir build
g++ -std=c++14 -Wall -Wextra -Iinclude ^
    -D_WIN32_WINNT=0x0600 ^
    -DWIN32_LEAN_AND_MEAN ^
    src\socket_wrapper.cpp ^
    src\server.cpp ^
    src\client.cpp ^
    src\network_manager.cpp ^
    src\message.cpp ^
    src\chat_session.cpp ^
    src\client_handler.cpp ^
    src\room.cpp ^
    src\main.cpp ^
    -o build\LAN_Chat.exe ^
    -lws2_32
if %ERRORLEVEL% == 0 (
    echo [Build] SUCCESS: build\LAN_Chat.exe created.
) else (
    echo [Build] FAILED. Check errors above.
)
