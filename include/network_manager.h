#pragma once
/**
 * @file network_manager.h
 * @brief Manages the active chat connection with a dedicated receive thread.
 *
 * NetworkManager owns a SocketWrapper and spawns a background thread that
 * continuously reads incoming messages and invokes a user-supplied callback.
 * Sending is done from the calling thread (thread-safe via mutex).
 *
 * Usage:
 *   NetworkManager nm(std::move(socket));
 *   nm.set_on_message([](const std::string& msg){ ... });
 *   nm.start();
 *   nm.send("Hello!");
 *   nm.stop();
 */

#include "socket_wrapper.h"

#include "compat.h"

#include <atomic>
#include <functional>
#include <string>

/**
 * @class NetworkManager
 * @brief Thread-safe wrapper that drives send/receive over a SocketWrapper.
 */
class NetworkManager {
public:
  /// Callback type invoked on the receive thread when a message arrives.
  using MessageCallback = std::function<void(const std::string &message)>;

  /**
   * @brief Construct with an already-connected socket.
   * @param socket A moved-in SocketWrapper (server or client side).
   */
  explicit NetworkManager(SocketWrapper socket);

  ~NetworkManager();

  // Non-copyable, non-movable
  NetworkManager(const NetworkManager &) = delete;
  NetworkManager &operator=(const NetworkManager &) = delete;

  /**
   * @brief Register the callback invoked when a message is received.
   * Must be called before start().
   */
  void set_on_message(MessageCallback cb);

  /**
   * @brief Register the callback invoked when the peer disconnects.
   */
  void set_on_disconnect(std::function<void()> cb);

  /// Start the background receive thread.
  void start();

  /**
   * @brief Send a message to the remote peer (thread-safe).
   * @param message UTF-8 text to send.
   */
  void send(const std::string &message);

  /**
   * @brief Stop the receive thread and close the socket.
   * Blocks until the receive thread exits.
   */
  void stop();

  /// @return true if the connection is still active.
  bool is_running() const { return running_.load(); }

private:
  SocketWrapper socket_;
  Thread recv_thread_;
  std::atomic<bool> running_{false};
  Mutex send_mutex_;

  MessageCallback on_message_;
  std::function<void()> on_disconnect_;

  /// Entry point for the background receive thread.
  void receive_loop();
};
