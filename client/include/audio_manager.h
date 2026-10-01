#pragma once

#include "raylib.h"

class AudioManager {
public:
  void initAudio();
  void updateBackgroundMusic();
  void updateMusicVolume(bool isPlaying);
  void unloadAudio();

  void playShootSound();
  void playExplosionSound();
  void playBossHitSound();

  bool isAudioReady();

private:
  Sound shoot_sfx_;
  Sound explosion_sfx_;
  Sound boss_hit_sfx_;
  Music background_music_;
  bool music_loaded_ = false;
  bool audio_ready_ = false;
  float music_volume_ = 0.0f;
  float target_music_volume_ = 0.0f;
  static constexpr float MUSIC_FADE_SPEED = 1.0f; // volume units per second
};
