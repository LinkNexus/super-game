#pragma once

// Forward-declare ixwebsocket types to avoid forcing the header to include the
// full third-party headers (which the editor indexer may not resolve).
namespace ix {
class WebSocket;
struct WebSocketMessagePtr;
enum class WebSocketMessageType : int;
struct SocketTLSOptions;
}

#include <functional>
#include <memory>
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
