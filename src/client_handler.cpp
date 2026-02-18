/**
 * @file client_handler.cpp
 * @brief Implementation of ClientHandler – per-connection receive thread.
 */

#include "client_handler.h"

#include <iostream>

// ── Construction / Destruction
// ────────────────────────────────────────────────

ClientHandler::ClientHandler(uint32_t id, std::string name,
                             SocketWrapper socket, MessageCallback on_msg,
                             DisconnectCallback on_disc)
    : id_(id), name_(std::move(name)), socket_(std::move(socket)),
      on_message_(std::move(on_msg)), on_disconnect_(std::move(on_disc)) {
  running_.store(true);
  recv_thread_ = Thread(&ClientHandler::receive_loop, this);
}

ClientHandler::~ClientHandler() {
  stop();
  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }
}

// ── Public API
// ────────────────────────────────────────────────────────────────

void ClientHandler::send(const std::string &message) {
  LockGuard<Mutex> lock(send_mutex_);
  if (socket_.is_valid()) {
    try {
      socket_.send_message(message);
    } catch (...) {
      // Ignore send errors — disconnect will be detected by recv loop
    }
  }
}

void ClientHandler::stop() {
  running_.store(false);
  socket_.close(); // unblocks recv_all in the receive thread
}

// ── Private: receive loop
// ─────────────────────────────────────────────────────

void ClientHandler::receive_loop() {
  while (running_.load()) {
    std::string msg;
    try {
      msg = socket_.receive_message();
    } catch (...) {
      break;
    }

    if (msg.empty()) {
      break; // peer disconnected
    }

    if (on_message_) {
      on_message_(id_, name_, msg);
    }
  }

  running_.store(false);

  if (on_disconnect_) {
    on_disconnect_(id_);
  }
}
