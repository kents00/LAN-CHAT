/**
 * @file message.cpp
 * @brief Implementation of Message – chat message value type.
 */

#include "message.h"

#include <ctime>
#include <iomanip>
#include <sstream>

// ── Construction
// ──────────────────────────────────────────────────────────────

Message::Message(std::string sender, std::string content)
    : sender(std::move(sender)), content(std::move(content)),
      timestamp(std::chrono::system_clock::now()) {}

// ── Formatting
// ────────────────────────────────────────────────────────────────

std::string Message::format() const {
  // Convert timestamp to local time
  std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
  std::tm local_tm{};
  // localtime_s is MSVC-only; localtime is portable and safe here
  // (we copy the result immediately before any other call)
  std::tm *tmp = std::localtime(&t);
  if (tmp)
    local_tm = *tmp;

  std::ostringstream oss;
  oss << "[" << std::setw(2) << std::setfill('0') << local_tm.tm_hour << ":"
      << std::setw(2) << std::setfill('0') << local_tm.tm_min << ":"
      << std::setw(2) << std::setfill('0') << local_tm.tm_sec << "] " << sender
      << ": " << content;
  return oss.str();
}
