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

#include <atomic>
#include <csignal>
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
            << "|       LAN Chat  v2.0                 |\n"
            << "|  Multi-PC group chat over LAN        |\n"
            << "+--------------------------------------+\n"
            << ansi::RESET << "\n";
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
    std::size_t count = room.client_count() + 1;
    std::cout << "\n"
              << ansi::GREEN << "[Server] New client connected: " << ip
              << "  (total: " << count << ")\n"
              << ansi::RESET << "You: " << std::flush;
    room.add_client(std::move(sock), ip);
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
  std::string host;
  std::cout << "\n"
            << ansi::CYAN
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

  std::cout << ansi::GREEN << "[Client] Connected! You are in the group chat.\n"
            << ansi::YELLOW << "  Type 'quit' or Ctrl+C to disconnect.\n"
            << ansi::RESET << "\n";

  ChatSession session;
  NetworkManager nm(std::move(conn));

  nm.set_on_message([&session](const std::string &text) {
    // text already contains "[SenderName]: message" from the server
    Message msg("", text); // raw formatted text from server
    session.add(msg);
    std::cout << "\r" << ansi::MAGENTA << text << ansi::RESET << "\n"
              << ansi::GREEN << "You" << ansi::RESET << ": " << std::flush;
  });

  nm.set_on_disconnect([]() {
    std::cout << "\n"
              << ansi::YELLOW << "[Chat] Server disconnected.\n"
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
