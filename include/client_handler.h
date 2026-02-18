#pragma once
/**
 * @file client_handler.h
 * @brief Manages one connected client in the multi-PC chat room.
 *
 * Each accepted client connection gets its own ClientHandler, which owns
 * the socket and a background receive thread. When a message arrives it
 * invokes a broadcast callback so the Room can forward it to all other clients.
 */

#include "socket_wrapper.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

/**
 * @class ClientHandler
 * @brief Owns one peer connection and drives its receive loop.
 *
 * Non-copyable, non-movable (owns a live thread).
 */
class ClientHandler {
public:
  /**
   * @brief Callback invoked on the receive thread when a message arrives.
   * @param handler_id  Unique ID of this handler (for exclusion in broadcast).
   * @param sender_name Display name of the sender (e.g. "192.168.1.11").
   * @param message     The received text.
   */
  using MessageCallback =
      std::function<void(uint32_t handler_id, const std::string &sender_name,
                         const std::string &message)>;

  /**
   * @brief Callback invoked when the peer disconnects.
   * @param handler_id Unique ID of this handler so the Room can remove it.
   */
  using DisconnectCallback = std::function<void(uint32_t handler_id)>;

  /**
   * @brief Construct and immediately start the receive thread.
   * @param id       Unique identifier assigned by the Room.
   * @param name     Human-readable name (peer IP or nickname).
   * @param socket   Moved-in connected socket.
   * @param on_msg   Called when a message is received.
   * @param on_disc  Called when the peer disconnects.
   */
  ClientHandler(uint32_t id, std::string name, SocketWrapper socket,
                MessageCallback on_msg, DisconnectCallback on_disc);

  ~ClientHandler();

  // Non-copyable, non-movable
  ClientHandler(const ClientHandler &) = delete;
  ClientHandler &operator=(const ClientHandler &) = delete;

  /// Send a message to this client (thread-safe).
  void send(const std::string &message);

  /// @return Unique ID of this handler.
  uint32_t id() const { return id_; }

  /// @return Display name (peer IP / nickname).
  const std::string &name() const { return name_; }

  /// @return true if the receive thread is still running.
  bool is_active() const { return running_.load(); }

  /// Request the receive thread to stop (does not block).
  void stop();

private:
  uint32_t id_;
  std::string name_;
  SocketWrapper socket_;
  std::atomic<bool> running_{false};
  std::mutex send_mutex_;

  MessageCallback on_message_;
  DisconnectCallback on_disconnect_;
  std::thread recv_thread_;

  /// Background receive loop entry point.
  void receive_loop();
};
