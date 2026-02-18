/**
 * @file network_manager.cpp
 * @brief Implementation of NetworkManager – drives send/receive with threads.
 */

#include "network_manager.h"

#include <stdexcept>

// ── Construction / Destruction
// ────────────────────────────────────────────────

NetworkManager::NetworkManager(SocketWrapper socket)
    : socket_(std::move(socket)) {}

NetworkManager::~NetworkManager() { stop(); }

// ── Configuration
// ─────────────────────────────────────────────────────────────

void NetworkManager::set_on_message(MessageCallback cb) {
  on_message_ = std::move(cb);
}

void NetworkManager::set_on_disconnect(std::function<void()> cb) {
  on_disconnect_ = std::move(cb);
}

// ── Lifecycle
// ─────────────────────────────────────────────────────────────────

void NetworkManager::start() {
  if (running_.load())
    return;
  running_.store(true);
  recv_thread_ = std::thread(&NetworkManager::receive_loop, this);
}

void NetworkManager::stop() {
  if (!running_.load())
    return;
  running_.store(false);
  socket_.close(); // unblocks recv_all in the receive thread
  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }
}

// ── Send
// ──────────────────────────────────────────────────────────────────────

void NetworkManager::send(const std::string &message) {
  LockGuard<Mutex> lock(send_mutex_);
  if (socket_.is_valid()) {
    socket_.send_message(message);
  }
}

// ── Private: receive loop
// ─────────────────────────────────────────────────────

void NetworkManager::receive_loop() {
  while (running_.load()) {
    std::string msg;
    try {
      msg = socket_.receive_message();
    } catch (...) {
      // Socket error – treat as disconnect
      break;
    }

    if (msg.empty()) {
      // Peer disconnected gracefully
      break;
    }

    if (on_message_) {
      on_message_(msg);
    }
  }

  running_.store(false);

  if (on_disconnect_) {
    on_disconnect_();
  }
}
