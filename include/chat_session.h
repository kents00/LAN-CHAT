#pragma once
/**
 * @file chat_session.h
 * @brief Stores the in-memory message history for the current chat session.
 *
 * Thread-safe: add() may be called from the receive thread while the main
 * thread reads history.
 */

#include "message.h"

#include <mutex>
#include <vector>

/**
 * @class ChatSession
 * @brief Accumulates Message objects and can print the full history.
 */
class ChatSession {
public:
  ChatSession() = default;

  /**
   * @brief Add a message to the history (thread-safe).
   * @param msg The Message to store.
   */
  void add(const Message &msg);

  /**
   * @brief Print all stored messages to stdout.
   * Useful for showing history after reconnect or on startup.
   */
  void print_history() const;

  /// @return Number of messages stored.
  std::size_t size() const;

private:
  mutable std::mutex mutex_;
  std::vector<Message> history_;
};
