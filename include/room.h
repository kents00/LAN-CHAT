#pragma once
/**
 * @file room.h
 * @brief Thread-safe registry of all connected clients; handles broadcast.
 *
 * The Room is the heart of multi-PC chat. The Server accept loop calls
 * add_client() for every new connection. When any ClientHandler receives
 * a message it calls Room::broadcast(), which forwards the message to
 * every other active client.
 *
 * Usage:
 *   Room room;
 *   room.add_client(std::move(socket), "192.168.1.11");
 *   room.broadcast(sender_id, "Alice", "Hello everyone!");
 *   room.broadcast_all("Server", "Server is shutting down.");
 */

#include "client_handler.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * @class Room
 * @brief Owns all ClientHandler objects and provides broadcast messaging.
 *
 * Thread-safe: add_client, remove_client, broadcast, and broadcast_all
 * may all be called from different threads simultaneously.
 */
class Room {
public:
  Room() = default;
  ~Room();

  // Non-copyable
  Room(const Room &) = delete;
  Room &operator=(const Room &) = delete;

  /**
   * @brief Register a new connected client.
   * @param socket Connected socket (moved in).
   * @param name   Display name for this client (e.g. peer IP).
   * @return The unique ID assigned to the new client.
   */
  uint32_t add_client(SocketWrapper socket, const std::string &name);

  /**
   * @brief Remove a client by ID (called from disconnect callback).
   * @param id The handler ID to remove.
   */
  void remove_client(uint32_t id);

  /**
   * @brief Send a message to all clients EXCEPT the sender.
   * @param sender_id   ID of the originating ClientHandler (excluded).
   * @param sender_name Display name prepended to the message.
   * @param message     Raw message text.
   */
  void broadcast(uint32_t sender_id, const std::string &sender_name,
                 const std::string &message);

  /**
   * @brief Send a message to ALL connected clients (e.g. server's own
   * messages).
   * @param sender_name Display name prepended to the message.
   * @param message     Raw message text.
   */
  void broadcast_all(const std::string &sender_name,
                     const std::string &message);

  /// @return Number of currently active clients.
  std::size_t client_count() const;

  /// Stop all client handlers (called on server shutdown).
  void stop_all();

private:
  mutable std::mutex mutex_;
  std::unordered_map<uint32_t, std::unique_ptr<ClientHandler>> clients_;
  uint32_t next_id_{1};
};
