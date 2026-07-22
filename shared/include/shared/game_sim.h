#include "shared/constants.h"
#include "shared/enemy_sim.h"
#include "shared/messages.h"
#include "shared/player_sim.h"

namespace shared {
enum class GameOutcome : uint8_t { NONE, WON, LOST };

class GameSim {
public:
  void start();
  void step(GameState &state, float dt);

private:
  void initEnemies();
  void updateEnemies(float dt);
  void checkCollisions();
  void initBoss();
  void animateBossEntrance(float dt);
  void updateBoss(float dt);

  PlayerSimState player_;
  std::array<BulletSimState, MAX_BULLETS> bullets_pool_;
  std::array<EnemySimState, ENEMIES_ROWS * ENEMIES_COLS> enemies_pool_;
  bool is_paused = false;
};
} // namespace shared
