#include "game.h"
#include "shared/constants.h"
#include "shared/rnd_generator.h"

int main(int argc, char *argv[]) {
  RndGenerator::seed();

  std::string server_url = shared::default_server_url;
  if (argc > 1)
    server_url = argv[1];

  Game game(server_url);
  game.run();
  return 0;
}
