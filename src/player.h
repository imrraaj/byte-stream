#ifndef PLAYER_H
#define PLAYER_H

#include "decoder.h"
#include "raylib.h"
#include "font.h"
#include "application.h"
#include "macros.h"

struct PlayerState
{
  Texture texture;
  AudioStream raylib_audio_stream;
  char *file_title;
  float volume;
  bool is_playing;
  bool is_muted;
};
typedef enum
{
  SINGLE_CLICK,
  DOUBLE_CLICK,
  BUTTON_CLICK_NONE
} MouseButtonPressedTimes;

typedef struct PlayerState PlayerState;
extern PlayerState ps;
extern int64_t frame_time;
extern Application app;

void audio_callback(void *buffer, unsigned int frames);
int player_init(char *filename);
void player_update(void);
void player_close(void);
#endif // PLAYER_H
