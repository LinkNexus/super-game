#include "audio_manager.h"
#include "raylib.h"
#include "shared/constants.h"
#include <algorithm>

void AudioManager::initAudio() {
  InitAudioDevice();
  if (IsAudioDeviceReady()) {
    audio_ready_ = true;
    shoot_sfx_ = LoadSound("assets/shoot.wav");
    explosion_sfx_ = LoadSound("assets/explosion.ogg");
    boss_hit_sfx_ = LoadSound("assets/boss_hit.ogg");
    background_music_ = LoadMusicStream("assets/backgroundsound.ogg");
    if (IsMusicValid(background_music_)) {
      music_loaded_ = true;
      target_music_volume_ = 0.0f;
      music_volume_ = 0.0f;
      SetMusicVolume(background_music_, 0.0f);
      PlayMusicStream(background_music_);
    }
  } else {
    audio_ready_ = false;
  }
}

void AudioManager::updateMusicVolume(bool isPlaying) {
  if (audio_ready_ && music_loaded_) {
    target_music_volume_ = isPlaying ? 0.25f : 0.7f;

    if (music_volume_ < target_music_volume_) {
      music_volume_ =
          std::min(music_volume_ + MUSIC_FADE_SPEED * shared::FIXED_DT,
                   target_music_volume_);
    } else if (music_volume_ > target_music_volume_) {
      music_volume_ =
          std::max(music_volume_ - MUSIC_FADE_SPEED * shared::FIXED_DT,
                   target_music_volume_);
    }
    SetMusicVolume(background_music_, music_volume_);
  }
}

void AudioManager::playShootSound() {
  if (audio_ready_)
    PlaySound(shoot_sfx_);
}

void AudioManager::playExplosionSound() {
  if (audio_ready_)
    PlaySound(explosion_sfx_);
}

void AudioManager::playBossHitSound() {
  if (audio_ready_)
    PlaySound(boss_hit_sfx_);
}

void AudioManager::unloadAudio() {
  if (audio_ready_) {
    UnloadSound(shoot_sfx_);
    UnloadSound(explosion_sfx_);
    UnloadSound(boss_hit_sfx_);
    if (music_loaded_)
      UnloadMusicStream(background_music_);
    CloseAudioDevice();
  }
}

void AudioManager::updateBackgroundMusic() {
  if (audio_ready_ && music_loaded_) {
    UpdateMusicStream(background_music_);
  }
}

bool AudioManager::isAudioReady() { return audio_ready_; }
