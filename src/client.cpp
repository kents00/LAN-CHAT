/**
 * @file client.cpp
 * @brief Implementation of Client – TCP connector using Winsock2.
 */

#include "client.h"

#include <stdexcept>
#include <string>
#include <ws2tcpip.h>

// ── Static helper
// ─────────────────────────────────────────────────────────────

void Client::init_winsock() {
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

// ── Construction
// ──────────────────────────────────────────────────────────────

Client::Client() { init_winsock(); }

// ── Public API
// ────────────────────────────────────────────────────────────────

SocketWrapper Client::connect_to(const std::string &host, unsigned short port) {
  // Resolve the host address
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo *result = nullptr;
  std::string port_str = std::to_string(port);

  int err = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
  if (err != 0) {
    throw std::runtime_error("getaddrinfo() failed for host '" + host +
                             "': " + std::to_string(err));
  }

  // Try each resolved address until one connects
  SOCKET sock = INVALID_SOCKET;
  for (addrinfo *ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock == INVALID_SOCKET)
      continue;

    if (::connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) ==
        SOCKET_ERROR) {
      ::closesocket(sock);
      sock = INVALID_SOCKET;
      continue;
    }
    break; // connected successfully
  }

  ::freeaddrinfo(result);

  if (sock == INVALID_SOCKET) {
    throw std::runtime_error("connect() failed to " + host + ":" + port_str +
                             " (error: " + std::to_string(WSAGetLastError()) +
                             ")");
  }

  return SocketWrapper(sock);
}
