/**
 * @file room.cpp
 * @brief Implementation of Room – broadcast hub for multi-PC chat.
 */

#include "room.h"

#include <iostream>

// ── Destruction
// ───────────────────────────────────────────────────────────────

Room::~Room() { stop_all(); }

// ── Client management
// ─────────────────────────────────────────────────────────

uint32_t Room::add_client(SocketWrapper socket, const std::string &name) {
  LockGuard<Mutex> lock(mutex_);

  uint32_t id = next_id_++;

  // Build callbacks that capture 'this' (Room outlives all handlers)
  auto on_msg = [this](uint32_t sender_id, const std::string &sender_name,
                       const std::string &message) {
    // Print on server console
    std::cout << "\r[" << sender_name << "]: " << message << "\n"
              << "You: " << std::flush;
    // Forward to all other clients
    broadcast(sender_id, sender_name, message);
  };

  auto on_disc = [this](uint32_t disc_id) { remove_client(disc_id); };

  auto handler = std::make_unique<ClientHandler>(
      id, name, std::move(socket), std::move(on_msg), std::move(on_disc));

  clients_.emplace(id, std::move(handler));
  return id;
}

void Room::remove_client(uint32_t id) {
  LockGuard<Mutex> lock(mutex_);
  auto it = clients_.find(id);
  if (it != clients_.end()) {
    std::cout << "\n[Room] " << it->second->name()
              << " disconnected. Active clients: " << (clients_.size() - 1)
              << "\n"
              << "You: " << std::flush;
    // stop() closes the socket; the thread will exit on its own
    it->second->stop();
    clients_.erase(it);
  }
}

// ── Broadcast
// ─────────────────────────────────────────────────────────────────

void Room::broadcast(uint32_t sender_id, const std::string &sender_name,
                     const std::string &message) {
  // Format: "[SenderName]: message"
  std::string payload = "[" + sender_name + "]: " + message;

  LockGuard<Mutex> lock(mutex_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->first != sender_id && it->second->is_active()) {
      it->second->send(payload);
    }
  }
}

void Room::broadcast_all(const std::string &sender_name,
                         const std::string &message) {
  std::string payload = "[" + sender_name + "]: " + message;

  LockGuard<Mutex> lock(mutex_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->second->is_active()) {
      it->second->send(payload);
    }
  }
}

// ── Utilities
// ─────────────────────────────────────────────────────────────────

std::size_t Room::client_count() const {
  LockGuard<Mutex> lock(mutex_);
  return clients_.size();
}

void Room::stop_all() {
  LockGuard<Mutex> lock(mutex_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    it->second->stop();
  }
  clients_.clear();
}
