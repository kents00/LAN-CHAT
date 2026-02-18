#pragma once
/**
 * @file server.h
 * @brief TCP server that listens on a port and accepts multiple client
 * connections.
 *
 * Usage (multi-client):
 *   Server srv(54000);
 *   srv.set_on_new_client([](SocketWrapper s, std::string ip){ ... });
 *   srv.start_accept_loop();   // runs in background thread
 *   ...
 *   srv.stop();
 *
 * Usage (single-client, legacy):
 *   SocketWrapper conn = srv.accept_client();  // blocks
 */

#include "socket_wrapper.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

/**
 * @class Server
 * @brief Binds a TCP socket to a port and accepts incoming connections.
 *
 * Supports both blocking single-accept and non-blocking continuous accept loop.
 */
class Server {
public:
  /**
   * @brief Callback invoked for each new accepted connection.
   * @param socket  The accepted connection socket.
   * @param peer_ip The remote IP address string.
   */
  using NewClientCallback =
      std::function<void(SocketWrapper socket, std::string peer_ip)>;

  /**
   * @brief Construct and start listening on the given port.
   * @param port TCP port to bind (default: DEFAULT_PORT).
   * @throws std::runtime_error if bind/listen fails.
   */
  explicit Server(unsigned short port = DEFAULT_PORT);

  ~Server();

  // Non-copyable, non-movable
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  /**
   * @brief Register the callback invoked for each new client (multi-client
   * mode). Must be set before calling start_accept_loop().
   */
  void set_on_new_client(NewClientCallback cb);

  /**
   * @brief Start a background thread that continuously accepts new clients.
   * Calls on_new_client callback for each accepted connection.
   */
  void start_accept_loop();

  /**
   * @brief Stop the accept loop and close the listening socket.
   * Blocks until the accept thread exits.
   */
  void stop();

  /**
   * @brief Block until one client connects and return the connection socket.
   * (Single-client / legacy mode — does not use the callback.)
   * @return A SocketWrapper representing the accepted connection.
   * @throws std::runtime_error on accept failure.
   */
  SocketWrapper accept_client();

  /// @return The port this server is listening on.
  unsigned short port() const { return port_; }

  /// @return true if the accept loop is running.
  bool is_running() const { return running_.load(); }

private:
  SOCKET listen_sock_;
  unsigned short port_;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
  NewClientCallback on_new_client_;

  /// Background accept loop entry point.
  void accept_loop();

  /// Initialise Winsock (called once in constructor).
  static void init_winsock();

  /// Get the peer IP string from an accepted sockaddr_in.
  static std::string peer_ip(const sockaddr_in &addr);
};
