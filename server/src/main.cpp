#include "shared/messages.h"
#include <cstdio>

int main(int argc, char *argv[]) {
  shared::GameState state{};

  std::printf("supergame_server stub, tick=%u\n", state.tick);

  return 0;
}
