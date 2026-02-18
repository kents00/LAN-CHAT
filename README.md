# LAN Chat v2.0

A lightweight, multi-PC group chat application written in **C++17** for the same local network. The server acts as a hub, allowing any number of clients to connect and talk simultaneously. No internet required — just a shared Wi-Fi or Ethernet connection.

---

## Features

- **Multi-PC Group Chat** (v2.0) — one PC acts as a hub; unlimited clients can join.
- **Broadcast System** — signals from any peer are sent to everyone in the room.
- **Bidirectional Messaging** — send and receive simultaneously via dedicated threads.
- **Timestamped messages** — every message shows `[HH:MM:SS] Sender: text`.
- **Coloured console output** — ANSI colours (Windows 10+).
- **Graceful shutdown** — the server can safely stop, or individual clients can quit.
- **No external dependencies** — built using only Winsock2 (Windows-native).

---

## Requirements

| Tool | Version |
|------|---------|
| CMake | ≥ 3.16 |
| C++ compiler | MinGW-w64 (GCC 10+) **or** MSVC 2019+ |
| OS | Windows 10 or later |

> **Note:** Both PCs must be on the same Wi-Fi or Ethernet network.

---

## Build Instructions

### Option A – MinGW (Recommended)

1. Install [MinGW-w64](https://winlibs.com/) and add its `bin/` folder to your `PATH`.
2. Open a command prompt in the project folder.
3. Run the one-click build script:
   ```bat
   build.bat
   ```

The executable `LAN_Chat.exe` will be generated in the `build/` folder.

### Option B – CMake + MinGW

```bat
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

---

## Usage

### Step 1 – Find the Server PC's IP

On the PC that will host the chat (the server), run:

```bat
ipconfig
```

Note the **IPv4 Address** (e.g., `192.168.1.10`).

### Step 2 – Start the Server (The Hub)

```bat
LAN_Chat.exe
```

Choose **S** (Server). The server will display your LAN IP addresses and wait for clients. You can also type messages here to broadcast to everyone.

### Step 3 – Connect Clients

On any other PC in the same network:

1. Run `LAN_Chat.exe`.
2. Choose **C** (Client).
3. Enter the server's IP address (e.g., `192.168.1.10`).

### Step 4 – Chat!

When any client or the server types a message, it is instantly broadcast to all other participants.

```
[192.168.1.11]: Hello from PC2!    ← Message appears for everyone
You: Hi PC2!                        ← Your reply
```

---

## Single-PC Testing (Loopback)

Test on one machine using multiple terminal windows:

1. **Terminal 1**: Run `LAN_Chat.exe` → choose **S**.
2. **Terminal 2**: Run `LAN_Chat.exe` → choose **C** → enter `127.0.0.1`.
3. **Terminal 3**: Run `LAN_Chat.exe` → choose **C** → enter `127.0.0.1`.

---

## Architecture (Hub-and-Spoke)

```
PC2 (Client) ──┐
PC3 (Client) ──┤──► Server PC (Hub) ──► Broadcasts to all others
PC4 (Client) ──┘
```

The server manages a `Room` object that tracks every connected `ClientHandler`. When a message arrives from any client, the server forwards it to every other active participant.

---

## Project Structure

```
CPP PING BOTH PC/
├── CMakeLists.txt          # Build configuration
├── build.bat               # Convenience build script
├── README.md               # This file
├── include/
│   ├── socket_wrapper.h    # RAII socket wrapper
│   ├── server.h            # Multi-client TCP listener
│   ├── client.h            # TCP connector
│   ├── network_manager.h   # Client-side thread manager
│   ├── room.h              # Hub/Broadcast registry (NEW)
│   ├── client_handler.h    # Server-side connection handler (NEW)
│   ├── message.h           # Message value type
│   └── chat_session.h      # Message history
└── src/
    ├── main.cpp            # Entry point (Server/Client logic)
    ├── socket_wrapper.cpp
    ├── server.cpp
    ├── client.cpp
    ├── network_manager.cpp
    ├── room.cpp
    ├── client_handler.cpp
    ├── message.cpp
    └── chat_session.cpp
```

---

## License

MIT – free to use and modify for educational purposes.
