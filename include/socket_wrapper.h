#pragma once
/**
 * @file socket_wrapper.h
 * @brief RAII wrapper around a Winsock SOCKET handle.
 *
 * Provides length-prefixed message framing so that each call to
 * send_message() / receive_message() transfers exactly one logical
 * chat message, regardless of TCP segmentation.
 *
 * Wire format (per message):
 *   [4 bytes – uint32_t length (network byte order)] [<length> bytes – UTF-8 text]
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <stdexcept>

/// Default TCP port used by both server and client.
constexpr unsigned short DEFAULT_PORT = 54000;

/**
 * @class SocketWrapper
 * @brief Owns a Winsock SOCKET and exposes simple string send/receive.
 *
 * Move-only (non-copyable) to enforce single ownership of the OS handle.
 */
class SocketWrapper {
public:
    /// Construct from an already-connected/accepted socket handle.
    explicit SocketWrapper(SOCKET sock);

    // Non-copyable
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;

    // Movable
    SocketWrapper(SocketWrapper&& other) noexcept;
    SocketWrapper& operator=(SocketWrapper&& other) noexcept;

    ~SocketWrapper();

    /**
     * @brief Send a UTF-8 string message to the remote peer.
     * @param message The text to send.
     * @throws std::runtime_error on socket error.
     */
    void send_message(const std::string& message);

    /**
     * @brief Block until a complete message is received from the remote peer.
     * @return The received UTF-8 string, or an empty string if the peer disconnected.
     * @throws std::runtime_error on socket error.
     */
    std::string receive_message();

    /// @return true if the underlying socket handle is valid.
    bool is_valid() const;

    /// Close the socket immediately.
    void close();

private:
    SOCKET sock_;

    /**
     * @brief Send exactly @p len bytes from @p buf.
     * @return false if the connection was closed gracefully.
     */
    bool send_all(const char* buf, int len);

    /**
     * @brief Receive exactly @p len bytes into @p buf.
     * @return false if the connection was closed gracefully.
     */
    bool recv_all(char* buf, int len);
};
