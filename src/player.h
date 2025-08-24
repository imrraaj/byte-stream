#ifndef PLAYER_H
#define PLAYER_H

#include "application.h"
#include "decoder.h"
#include "font.h"
#include "macros.h"
#include "raylib.h"

typedef struct {
    double last_hover_time;
    double last_volume_change_time;
    double last_audio_change_time;
    double last_subtitle_change_time;
    double last_ss_save_change_time;
    char audio_language[512];
    char subtitle_language[512];
    bool volumeSliderDragging;
    bool seekBarDragging;
    bool show_help_menu;
    float lastClickTime;
} UIState;

typedef struct {
    Texture2D play;
    Texture2D pause;
    Texture2D ff;
    Texture2D bb;
    Texture2D volume;
    Texture2D mute;
} UITextures;

struct PlayerState {
    Texture texture;
    AudioStream raylib_audio_stream;
    char *file_title;
    float volume;
    bool is_playing;
    bool is_muted;
};
typedef enum {
    SINGLE_CLICK,
    DOUBLE_CLICK,
    BUTTON_CLICK_NONE
} MouseButtonPressedTimes;

typedef struct PlayerState PlayerState;
extern PlayerState ps;
extern Application app;

void audio_callback(void *buffer, unsigned int frames);
int player_init(char *filename, UIState *ui_state, UITextures *ui_textures);
void player_update(UIState *ui_state, UITextures *ui_textures);
void player_close(UITextures *ui_textures);
#endif // PLAYER_H
