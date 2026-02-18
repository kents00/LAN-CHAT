#pragma once
/**
 * @file compat.h
 * @brief Compatibility layer for MinGW g++ 6.3.0 (win32 threading model).
 *
 * Provides Thread, Mutex, and LockGuard as replacements for std::thread,
 * std::mutex, and std::lock_guard which are unavailable when MinGW is
 * built with --threads=win32.
 *
 * Also provides a fallback inet_ntop for older MinGW ws2tcpip.h.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <process.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>


#include <cstring>
#include <functional>


// ── Thread (replaces std::thread) ──────────────────────────────────

class Thread {
public:
  Thread() : handle_(nullptr), id_(0) {}

  template <typename Fn, typename... Args>
  explicit Thread(Fn &&fn, Args &&...args) : handle_(nullptr), id_(0) {
    auto *task = new std::function<void()>(
        std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
    handle_ = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, &Thread::thread_func, task, 0, &id_));
  }

  ~Thread() {
    if (handle_)
      CloseHandle(handle_);
  }

  // Move-only
  Thread(Thread &&other) noexcept : handle_(other.handle_), id_(other.id_) {
    other.handle_ = nullptr;
    other.id_ = 0;
  }

  Thread &operator=(Thread &&other) noexcept {
    if (this != &other) {
      if (handle_)
        CloseHandle(handle_);
      handle_ = other.handle_;
      id_ = other.id_;
      other.handle_ = nullptr;
      other.id_ = 0;
    }
    return *this;
  }

  Thread(const Thread &) = delete;
  Thread &operator=(const Thread &) = delete;

  bool joinable() const { return handle_ != nullptr; }

  void join() {
    if (handle_) {
      WaitForSingleObject(handle_, INFINITE);
      CloseHandle(handle_);
      handle_ = nullptr;
    }
  }

private:
  HANDLE handle_;
  unsigned id_;

  static unsigned __stdcall thread_func(void *arg) {
    auto *task = static_cast<std::function<void()> *>(arg);
    (*task)();
    delete task;
    return 0;
  }
};

// ── Mutex (replaces std::mutex) ────────────────────────────────────

class Mutex {
public:
  Mutex() { InitializeCriticalSection(&cs_); }
  ~Mutex() { DeleteCriticalSection(&cs_); }

  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

  void lock() { EnterCriticalSection(&cs_); }
  void unlock() { LeaveCriticalSection(&cs_); }

private:
  CRITICAL_SECTION cs_;
};

// ── LockGuard (replaces std::lock_guard) ───────────────────────────

template <typename M> class LockGuard {
public:
  explicit LockGuard(M &m) : mutex_(m) { mutex_.lock(); }
  ~LockGuard() { mutex_.unlock(); }

  LockGuard(const LockGuard &) = delete;
  LockGuard &operator=(const LockGuard &) = delete;

private:
  M &mutex_;
};

// ── inet_ntop fallback ─────────────────────────────────────────────

#ifndef COMPAT_INET_NTOP_DEFINED
#define COMPAT_INET_NTOP_DEFINED
inline const char *compat_inet_ntop(int af, const void *src, char *dst,
                                    int size) {
  if (af == AF_INET) {
    struct in_addr addr;
    std::memcpy(&addr, src, sizeof(addr));
    const char *p = inet_ntoa(addr);
    if (p) {
      std::strncpy(dst, p, static_cast<size_t>(size));
      dst[size - 1] = '\0';
      return dst;
    }
  }
  return nullptr;
}
#endif
