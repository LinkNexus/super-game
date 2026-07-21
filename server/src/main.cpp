#include "App.h"
#include "shared/messages.h"
#include <cstdio>

int main(int argc, char *argv[]) {
  shared::GameState state{};

  std::printf("supergame_server stub, tick=%u\n", state.tick);

  uWS::App()
      .listen(9001,
              [](auto *token) {
                if (token) {
                  std::printf("uWebSockets listening on port 9001\n");
                } else {
                  std::printf("uWebSockets failed to listen on port 9001\n");
                }
              })
      .run();

  return 0;
}
