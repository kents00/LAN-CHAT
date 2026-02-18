/**
 * @file socket_wrapper.cpp
 * @brief Implementation of SocketWrapper – RAII Winsock socket with
 *        length-prefixed message framing.
 */

#include "socket_wrapper.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>

// ── Construction / Destruction
// ────────────────────────────────────────────────

SocketWrapper::SocketWrapper(SOCKET sock) : sock_(sock) {}

SocketWrapper::SocketWrapper(SocketWrapper &&other) noexcept
    : sock_(other.sock_) {
  other.sock_ = INVALID_SOCKET;
}

SocketWrapper &SocketWrapper::operator=(SocketWrapper &&other) noexcept {
  if (this != &other) {
    close();
    sock_ = other.sock_;
    other.sock_ = INVALID_SOCKET;
  }
  return *this;
}

SocketWrapper::~SocketWrapper() { close(); }

// ── Public API
// ────────────────────────────────────────────────────────────────

bool SocketWrapper::is_valid() const { return sock_ != INVALID_SOCKET; }

void SocketWrapper::close() {
  if (sock_ != INVALID_SOCKET) {
    ::shutdown(sock_, SD_BOTH);
    ::closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }
}

void SocketWrapper::send_message(const std::string &message) {
  if (!is_valid()) {
    throw std::runtime_error("send_message: socket is not valid");
  }

  // Encode the message length as a 4-byte big-endian uint32
  auto len = static_cast<uint32_t>(message.size());
  uint32_t net_len = htonl(len);

  if (!send_all(reinterpret_cast<const char *>(&net_len), sizeof(net_len))) {
    throw std::runtime_error("send_message: failed to send length header");
  }
  if (len > 0 && !send_all(message.data(), static_cast<int>(len))) {
    throw std::runtime_error("send_message: failed to send message body");
  }
}

std::string SocketWrapper::receive_message() {
  if (!is_valid()) {
    return {};
  }

  // Read 4-byte length header
  uint32_t net_len = 0;
  if (!recv_all(reinterpret_cast<char *>(&net_len), sizeof(net_len))) {
    return {}; // peer disconnected
  }

  uint32_t len = ntohl(net_len);
  if (len == 0) {
    return {};
  }

  // Guard against absurdly large messages (> 64 MB)
  constexpr uint32_t MAX_MSG = 64u * 1024u * 1024u;
  if (len > MAX_MSG) {
    throw std::runtime_error("receive_message: message too large");
  }

  std::string buf(len, '\0');
  if (!recv_all(&buf[0], static_cast<int>(len))) {
    return {}; // peer disconnected mid-message
  }
  return buf;
}

// ── Private Helpers
// ───────────────────────────────────────────────────────────

bool SocketWrapper::send_all(const char *buf, int len) {
  int sent = 0;
  while (sent < len) {
    int result = ::send(sock_, buf + sent, len - sent, 0);
    if (result == SOCKET_ERROR || result == 0) {
      return false;
    }
    sent += result;
  }
  return true;
}

bool SocketWrapper::recv_all(char *buf, int len) {
  int received = 0;
  while (received < len) {
    int result = ::recv(sock_, buf + received, len - received, 0);
    if (result == SOCKET_ERROR || result == 0) {
      return false;
    }
    received += result;
  }
  return true;
}
