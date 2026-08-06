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
