#pragma once

#include "ixwebsocket/IXWebSocket.h"
#include <functional>
#include <string>

/// Thin wrapper around an IXWebSocket client connection: connects to a
/// given URL and forwards received text messages to a callback. Messages
/// arrive on IXWebSocket's own background thread, not the caller's.
class NetworkClient {
  using MessageCallback = std::function<void(const std::string &)>;

public:
  NetworkClient(const std::string &url, MessageCallback onMessageCallback);
  ~NetworkClient();

  /// Opens the WebSocket connection asynchronously; `onMessageCallback` may
  /// start firing (on a background thread) before this call returns.
  void connect();

  /// Sends @p message as a text frame.
  /// @return false if not currently connected.
  bool send(const std::string &message);

private:
  std::string url_;
  std::unique_ptr<ix::WebSocket> ws_;
  MessageCallback onMessageCallback_;
};
