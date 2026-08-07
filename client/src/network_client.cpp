#include "network_client.h"
#include "ixwebsocket/IXNetSystem.h"
#include "ixwebsocket/IXWebSocket.h"
#include <iostream>

NetworkClient::NetworkClient(const std::string &url,
                             MessageCallback onMessageCallback)
    : url_(url), onMessageCallback_(onMessageCallback) {}

NetworkClient::~NetworkClient() {
  if (ws_) {
    ws_->close();
  }
}

void NetworkClient::connect() {
  if (ws_) {
    std::cout << "Already connected to server" << std::endl;
    return;
  }

  ix::initNetSystem();
  ws_ = std::make_unique<ix::WebSocket>();
  ws_->setUrl(url_);

  // IXWebSocket's mbedTLS backend can't load the OS trust store on macOS or
  // Linux (loadSystemCertificates() in IXSocketMbedTLS.cpp is an
  // unimplemented stub on both) - the default caFile="SYSTEM" silently
  // fails TLS connections there. Point it at a bundled CA cert file instead
  // (copied next to the binary alongside assets/); Windows doesn't need
  // this since its system-store path is actually implemented, but pointing
  // it at the same bundle everywhere is simpler than branching per OS.
  ix::SocketTLSOptions tlsOptions;
  tlsOptions.caFile = "assets/cacert.pem";
  ws_->setTLSOptions(tlsOptions);

  std::cout << "Connecting to " << url_ << "..." << std::endl;

  ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
    if (msg->type == ix::WebSocketMessageType::Open) {
      std::cout << "Connected to server" << std::endl;
    } else if (msg->type == ix::WebSocketMessageType::Close) {
      std::cout << "Disconnected from server" << std::endl;
    } else if (msg->type == ix::WebSocketMessageType::Message) {
      onMessageCallback_(msg->str);
    }
  });

  ws_->start();
}

bool NetworkClient::send(const std::string &message) {
  if (ws_) {
    ws_->send(message);
    return true;
  }
  return false;
}
