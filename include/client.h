#pragma once
/**
 * @file client.h
 * @brief TCP client that connects to a remote server by IP and port.
 *
 * Usage:
 *   Client cli;
 *   SocketWrapper conn = cli.connect_to("192.168.1.10", 54000);
 */

#include "socket_wrapper.h"
#include <string>

/**
 * @class Client
 * @brief Resolves a hostname/IP and establishes a TCP connection.
 */
class Client {
public:
    Client();
    ~Client() = default;

    // Non-copyable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /**
     * @brief Connect to a remote server.
     * @param host IPv4 address or hostname of the server.
     * @param port TCP port of the server (default: DEFAULT_PORT).
     * @return A SocketWrapper for the established connection.
     * @throws std::runtime_error if connection fails.
     */
    SocketWrapper connect_to(const std::string& host,
                             unsigned short port = DEFAULT_PORT);

private:
    /// Initialise Winsock (called once in constructor).
    static void init_winsock();
};
