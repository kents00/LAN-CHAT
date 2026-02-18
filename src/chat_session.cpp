/**
 * @file chat_session.cpp
 * @brief Implementation of ChatSession – thread-safe message history.
 */

#include "chat_session.h"

#include <iostream>

// ── Public API
// ────────────────────────────────────────────────────────────────

void ChatSession::add(const Message &msg) {
  LockGuard<Mutex> lock(mutex_);
  history_.push_back(msg);
}

void ChatSession::print_history() const {
  LockGuard<Mutex> lock(mutex_);
  for (const auto &msg : history_) {
    std::cout << msg.format() << "\n";
  }
}

std::size_t ChatSession::size() const {
  LockGuard<Mutex> lock(mutex_);
  return history_.size();
}
