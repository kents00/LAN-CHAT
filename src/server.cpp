/**
 * @file server.cpp
 * @brief Implementation of Server – multi-client TCP listener using Winsock2.
 */

#include "server.h"

#include <stdexcept>
#include <string>
#include <ws2tcpip.h>

// ── Static helpers
// ────────────────────────────────────────────────────────────

void Server::init_winsock() {
  static bool initialised = false;
  if (!initialised) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
      throw std::runtime_error("WSAStartup failed with code: " +
                               std::to_string(result));
    }
    initialised = true;
  }
}

std::string Server::peer_ip(const sockaddr_in &addr) {
  char ip[INET_ADDRSTRLEN] = {};
  compat_inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
  return std::string(ip);
}

// ── Construction / Destruction
// ────────────────────────────────────────────────

Server::Server(unsigned short port)
    : listen_sock_(INVALID_SOCKET), port_(port) {
  init_winsock();

  listen_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_sock_ == INVALID_SOCKET) {
    throw std::runtime_error("socket() failed: " +
                             std::to_string(WSAGetLastError()));
  }

  int opt = 1;
  ::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&opt), sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port_);

  if (::bind(listen_sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) ==
      SOCKET_ERROR) {
    ::closesocket(listen_sock_);
    throw std::runtime_error("bind() failed: " +
                             std::to_string(WSAGetLastError()));
  }

  // Backlog of 10 — allows up to 10 pending connections
  if (::listen(listen_sock_, 10) == SOCKET_ERROR) {
    ::closesocket(listen_sock_);
    throw std::runtime_error("listen() failed: " +
                             std::to_string(WSAGetLastError()));
  }
}

Server::~Server() { stop(); }

// ── Configuration
// ─────────────────────────────────────────────────────────────

void Server::set_on_new_client(NewClientCallback cb) {
  on_new_client_ = std::move(cb);
}

// ── Multi-client accept loop
// ──────────────────────────────────────────────────

void Server::start_accept_loop() {
  if (running_.load())
    return;
  running_.store(true);
  accept_thread_ = Thread(&Server::accept_loop, this);
}

void Server::stop() {
  if (!running_.load() && listen_sock_ == INVALID_SOCKET)
    return;
  running_.store(false);
  if (listen_sock_ != INVALID_SOCKET) {
    ::closesocket(listen_sock_);
    listen_sock_ = INVALID_SOCKET;
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
}

void Server::accept_loop() {
  while (running_.load()) {
    sockaddr_in client_addr{};
    int addr_len = sizeof(client_addr);

    SOCKET client_sock = ::accept(
        listen_sock_, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);

    if (client_sock == INVALID_SOCKET) {
      // Either stopped or real error
      break;
    }

    std::string ip = peer_ip(client_addr);

    if (on_new_client_) {
      on_new_client_(SocketWrapper(client_sock), ip);
    } else {
      ::closesocket(client_sock);
    }
  }
}

// ── Single-client (legacy)
// ────────────────────────────────────────────────────

SocketWrapper Server::accept_client() {
  sockaddr_in client_addr{};
  int addr_len = sizeof(client_addr);

  SOCKET client_sock = ::accept(
      listen_sock_, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);

  if (client_sock == INVALID_SOCKET) {
    throw std::runtime_error("accept() failed: " +
                             std::to_string(WSAGetLastError()));
  }

  return SocketWrapper(client_sock);
}
