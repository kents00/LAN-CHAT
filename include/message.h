#pragma once
/**
 * @file message.h
 * @brief Represents a single chat message with sender, content, and timestamp.
 */

#include <chrono>
#include <string>

/**
 * @struct Message
 * @brief Immutable value type for a chat message.
 */
struct Message {
  std::string sender;  ///< Display name of the sender ("You" or "Peer")
  std::string content; ///< UTF-8 message body
  std::chrono::system_clock::time_point
      timestamp; ///< When the message was created

  /**
   * @brief Construct a Message with the current time as timestamp.
   * @param sender  Sender display name.
   * @param content Message body.
   */
  Message(std::string sender, std::string content);

  /**
   * @brief Format the message for console display.
   * @return A string like "[12:34:56] You: Hello!"
   */
  std::string format() const;
};
