#pragma once

#include <mutex>

/// Single-slot, thread-safe mailbox used to hand a value from
/// `NetworkClient`'s background IXWebSocket thread to the main thread
/// polling it once per frame. A new `set()` overwrites any unread pending
/// value - only the latest matters for rendering.
template <typename T> class MailBox {
public:
  void set(T value) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_ = std::move(value);
    has_pending_ = true;
  }

  /// @return The pending value and clears it, or `std::nullopt` if nothing
  /// new has arrived since the last call.
  std::optional<T> take() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_pending_)
      return std::nullopt;
    has_pending_ = false;
    return std::move(pending_);
  }

private:
  std::mutex mutex_{};
  T pending_{};
  bool has_pending_{};
};
