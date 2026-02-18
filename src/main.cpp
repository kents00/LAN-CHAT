/**
 * @file main.cpp
 * @brief Entry point for LAN Chat v2.0 – multi-PC group chat over local
 * network.
 *
 * Modes:
 *   [S] Server – accepts unlimited clients, broadcasts messages between them.
 *                The server PC can also type messages (sent to all clients).
 *   [C] Client – connects to the server by IP, sends/receives messages.
 *
 * Type "quit" or press Ctrl+C to exit.
 */

// Winsock must be included before windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Older MinGW headers may not define this constant
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#include "chat_session.h"
#include "client.h"
#include "compat.h"
#include "message.h"
#include "network_manager.h"
#include "room.h"
#include "server.h"
#include "version.h"

#include <atomic>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

// ── ANSI colour helpers
// ───────────────────────────────────────────────────────
namespace ansi {
constexpr const char *RESET = "\033[0m";
constexpr const char *BOLD = "\033[1m";
constexpr const char *CYAN = "\033[36m";
constexpr const char *GREEN = "\033[32m";
constexpr const char *YELLOW = "\033[33m";
constexpr const char *RED = "\033[31m";
constexpr const char *MAGENTA = "\033[35m";
constexpr const char *CLEAR_LINE = "\033[2K\r"; // erase line + carriage return
} // namespace ansi

// ── Global shutdown flag
// ──────────────────────────────────────────────────────
static std::atomic<bool> g_shutdown{false};

/// Enable ANSI escape code processing on Windows 10+.
static void enable_ansi_console() {
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE)
    return;
  DWORD mode = 0;
  if (!GetConsoleMode(hOut, &mode))
    return;
  SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/// Signal handler for Ctrl+C.
static void signal_handler(int /*signal*/) { g_shutdown.store(true); }

/// Print the application banner.
static void print_banner() {
  std::cout << "\n"
            << ansi::CYAN << ansi::BOLD
            << "+--------------------------------------+\n"
            << "|       LAN Chat  v" << APP_VERSION << "               |\n"
            << "|  Multi-PC group chat over LAN        |\n"
            << "+--------------------------------------+\n"
            << ansi::RESET << "\n";
}

/// Get the full path to the currently running executable.
static std::string get_exe_path() {
  char buf[MAX_PATH] = {};
  GetModuleFileNameA(NULL, buf, MAX_PATH);
  return std::string(buf);
}

/// Get the directory containing the currently running executable.
static std::string get_exe_dir() {
  std::string path = get_exe_path();
  std::string::size_type pos = path.find_last_of("\\/");
  if (pos != std::string::npos) {
    return path.substr(0, pos);
  }
  return ".";
}

/// Print local LAN IP addresses.
static void print_local_ips() {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0)
    return;

  addrinfo hints{}, *res = nullptr;
  hints.ai_family = AF_INET;
  if (getaddrinfo(hostname, nullptr, &hints, &res) != 0)
    return;

  std::cout << ansi::YELLOW << "[Server] Your LAN IP address(es):\n";
  for (auto *p = res; p; p = p->ai_next) {
    char ip[INET_ADDRSTRLEN];
    auto *sa = reinterpret_cast<sockaddr_in *>(p->ai_addr);
    compat_inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
    std::cout << "           " << ip << "\n";
  }
  std::cout << ansi::RESET;
  freeaddrinfo(res);
}

// ── Server mode
// ───────────────────────────────────────────────────────────────

/**
 * @brief Run the server: accept unlimited clients, broadcast messages.
 *
 * The server's own stdin input is broadcast to ALL connected clients.
 * Client messages are forwarded to all OTHER clients and printed locally.
 */
static void run_server() {
  std::cout << "\n"
            << ansi::CYAN << "[Server] Starting on port " << DEFAULT_PORT
            << "...\n"
            << ansi::RESET;

  print_local_ips();

  Server server(DEFAULT_PORT);
  Room room;

  // Register callback: each new connection gets added to the Room
  server.set_on_new_client([&room](SocketWrapper sock, std::string ip) {
    // Read the first message as the client's chosen username
    std::string username;
    try {
      username = sock.receive_message();
    } catch (...) {
      username = "";
    }
    if (username.empty()) {
      username = ip;
    }

    // Read the second message — client version
    std::string client_version;
    try {
      client_version = sock.receive_message();
    } catch (...) {
      client_version = "";
    }

    // Check for version prefix
    const std::string ver_prefix = "CMD:VERSION:";
    std::string ver_str = "";
    if (client_version.size() >= ver_prefix.size() &&
        client_version.substr(0, ver_prefix.size()) == ver_prefix) {
      ver_str = client_version.substr(ver_prefix.size());
    }

    // Compare versions and send update if needed
    if (!ver_str.empty() && ver_str != std::string(APP_VERSION)) {
      // Client is outdated — read our own exe and send it
      std::string exe_path = get_exe_path();
      std::ifstream file(exe_path.c_str(), std::ios::binary | std::ios::ate);
      if (file.is_open()) {
        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string exe_data(static_cast<std::size_t>(file_size), '\0');
        file.read(&exe_data[0], file_size);
        file.close();

        // Tell client an update is available
        std::string cmd = "CMD:UPDATE:" + std::to_string(file_size);
        sock.send_message(cmd);

        // Send the exe binary
        sock.send_binary(exe_data.data(),
                         static_cast<uint32_t>(exe_data.size()));

        std::cout << ansi::CLEAR_LINE << ansi::YELLOW
                  << "[Server] Sent update (v" << APP_VERSION << ") to "
                  << username << " (was v" << ver_str << ")\n"
                  << ansi::RESET << "You: " << std::flush;
      } else {
        sock.send_message("CMD:OK");
      }
    } else {
      sock.send_message("CMD:OK");
    }

    std::size_t count = room.client_count() + 1;
    std::cout << ansi::CLEAR_LINE << ansi::GREEN << "[Server] " << username
              << " (" << ip << ") connected  (total: " << count << ")\n"
              << ansi::RESET << "You: " << std::flush;
    room.add_client(std::move(sock), username);
  });

  server.start_accept_loop();

  std::cout << ansi::CYAN
            << "[Server] Waiting for clients... (type messages to broadcast)\n"
            << ansi::YELLOW << "  Type 'quit' or Ctrl+C to shut down.\n"
            << ansi::RESET << "\n";

  // Server's own chat loop — broadcasts to all clients
  std::string line;
  while (!g_shutdown.load()) {
    std::cout << ansi::GREEN << "You" << ansi::RESET << ": " << std::flush;

    if (!std::getline(std::cin, line))
      break;
    if (line == "quit" || line == "exit")
      break;
    if (line.empty())
      continue;

    if (room.client_count() == 0) {
      std::cout << ansi::YELLOW << "[Server] No clients connected yet.\n"
                << ansi::RESET;
      continue;
    }

    room.broadcast_all("Server", line);
  }

  g_shutdown.store(true);

  std::cout << "\n"
            << ansi::CYAN << "[Server] Shutting down. Clients connected: "
            << room.client_count() << "\n"
            << ansi::RESET;

  server.stop();
  room.stop_all();
}

// ── Client mode
// ───────────────────────────────────────────────────────────────

/**
 * @brief Run the client: connect to server, send/receive messages.
 */
static void run_client() {
  std::string username;
  std::cout << "\n"
            << ansi::CYAN << "[Client] Enter your username: " << ansi::RESET;
  std::getline(std::cin, username);
  if (username.empty()) {
    username = "Anonymous";
  }

  std::string host;
  std::cout << ansi::CYAN
            << "[Client] Enter server IP address: " << ansi::RESET;
  std::getline(std::cin, host);

  if (host.empty()) {
    std::cerr << ansi::RED << "No IP entered. Exiting.\n" << ansi::RESET;
    return;
  }

  std::cout << ansi::CYAN << "[Client] Connecting to " << host << ":"
            << DEFAULT_PORT << "...\n"
            << ansi::RESET;

  Client client;
  SocketWrapper conn = client.connect_to(host, DEFAULT_PORT);

  std::cout << ansi::GREEN << "[Client] Connected as \"" << username << "\"!\n"
            << ansi::YELLOW << "  Type 'quit' or Ctrl+C to disconnect.\n"
            << ansi::RESET << "\n";

  ChatSession session;

  // ── Version handshake on raw socket (before creating NetworkManager)
  // Send username
  conn.send_message(username);

  // Send version
  conn.send_message(std::string("CMD:VERSION:") + APP_VERSION);

  // Read server response
  std::string server_response;
  try {
    server_response = conn.receive_message();
  } catch (...) {
    server_response = "";
  }

  if (server_response.substr(0, 11) == "CMD:UPDATE:") {
    // Server is sending us an updated exe
    std::cout << ansi::YELLOW
              << "[Update] New version available! Downloading...\n"
              << ansi::RESET << std::flush;

    std::string exe_data;
    if (conn.receive_binary(exe_data) && !exe_data.empty()) {
      // Save next to our own exe
      std::string save_path = get_exe_dir() + "\\LAN_Chat_new.exe";
      std::ofstream out_file(save_path.c_str(),
                             std::ios::binary | std::ios::trunc);
      if (out_file.is_open()) {
        out_file.write(exe_data.data(),
                       static_cast<std::streamsize>(exe_data.size()));
        out_file.close();
        std::cout << ansi::GREEN << "[Update] Saved as: " << save_path << "\n"
                  << "[Update] Close this app and run LAN_Chat_new.exe "
                  << "to use the latest version.\n"
                  << ansi::RESET << "\n";
      } else {
        std::cout << ansi::RED << "[Update] Failed to save update file.\n"
                  << ansi::RESET;
      }
    } else {
      std::cout << ansi::RED << "[Update] Failed to download update.\n"
                << ansi::RESET;
    }
  } else {
    std::cout << ansi::GREEN << "[Update] You are running the latest version (v"
              << APP_VERSION << ")\n"
              << ansi::RESET;
  }

  // ── Now hand socket to NetworkManager for normal chat
  NetworkManager nm(std::move(conn));

  nm.set_on_message([&session](const std::string &text) {
    // Normal chat message
    Message msg("", text);
    session.add(msg);
    std::cout << ansi::CLEAR_LINE << ansi::MAGENTA << text << ansi::RESET
              << "\n"
              << ansi::GREEN << "You" << ansi::RESET << ": " << std::flush;
  });

  nm.set_on_disconnect([]() {
    std::cout << ansi::CLEAR_LINE << ansi::YELLOW
              << "[Chat] Server disconnected.\n"
              << ansi::RESET;
    g_shutdown.store(true);
  });

  nm.start();

  // Client chat loop
  std::string line;
  while (!g_shutdown.load() && nm.is_running()) {
    std::cout << ansi::GREEN << "You" << ansi::RESET << ": " << std::flush;

    if (!std::getline(std::cin, line))
      break;
    if (line == "quit" || line == "exit")
      break;
    if (line.empty())
      continue;

    Message msg("You", line);
    session.add(msg);

    try {
      nm.send(line);
    } catch (const std::exception &e) {
      std::cerr << ansi::RED << "[Error] " << e.what() << ansi::RESET << "\n";
      break;
    }
  }

  g_shutdown.store(true);
  nm.stop();

  std::cout << "\n"
            << ansi::CYAN
            << "[Chat] Disconnected. Messages exchanged: " << session.size()
            << "\n"
            << ansi::RESET;
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  enable_ansi_console();
  print_banner();

  std::signal(SIGINT, signal_handler);

  // Mode selection
  char mode = '\0';
  while (mode != 'S' && mode != 'C') {
    std::cout << "Run as [S]erver or [C]lient? ";
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
      mode =
          static_cast<char>(std::toupper(static_cast<unsigned char>(input[0])));
    }
  }

  try {
    if (mode == 'S') {
      run_server();
    } else {
      run_client();
    }
  } catch (const std::exception &e) {
    std::cerr << ansi::RED << "[Fatal] " << e.what() << ansi::RESET << "\n";
    WSACleanup();
    return 1;
  }

  WSACleanup();
  return 0;
}
