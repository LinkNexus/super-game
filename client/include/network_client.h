#pragma once

#include "ixwebsocket/IXWebSocket.h"
#include <functional>
#include <string>

class NetworkClient {
  using MessageCallback = std::function<void(const std::string &)>;

public:
  NetworkClient(const std::string &url, MessageCallback onMessageCallback);
  ~NetworkClient();
  void connect();
  bool send(const std::string &message);

private:
  std::string url_;
  std::unique_ptr<ix::WebSocket> ws_;
  MessageCallback onMessageCallback_;
};
