#pragma once

#include "ixwebsocket/IXWebSocket.h"
#include "shared/wire.h"
#include <functional>
#include <string>

/// Thin wrapper around an IXWebSocket client connection: connects to a
/// given URL and forwards received text messages to a callback. Messages
/// arrive on IXWebSocket's own background thread, not the caller's.
class NetworkClient {
  using MessageCallback = std::function<void(const nlohmann::json &)>;

  static constexpr std::array<std::string_view, 2> PROTOCOLS = {
      shared::subprotocolName(shared::WireFormat::MSGPACK),
      shared::subprotocolName(shared::WireFormat::JSON)};

public:
  NetworkClient(const std::string &url, MessageCallback onMessageCallback);
  ~NetworkClient();

  /// Opens the WebSocket connection asynchronously; `onMessageCallback` may
  /// start firing (on a background thread) before this call returns.
  void connect();

  /// Sends @p message as a either a text or a binary frame.
  /// @return false if not currently connected.
  bool send(const nlohmann::json &envelope);

private:
  std::string url_;
  std::unique_ptr<ix::WebSocket> ws_;
  MessageCallback onMessageCallback_;
  std::atomic<shared::WireFormat> format_;
};
