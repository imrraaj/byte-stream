#include "player.h"
#include "decoder.h"
#include "raylib.h"
#include "raymath.h"
#include "bundle.h"
#include <math.h>
#include <stdio.h>

#define FONT_SIZE 36
#define ICON_SIZE 32 / 1.25
#define BUTTON_RADIUS ICON_SIZE * 1.5f

#define MIN_VOLUME_ALLOWED 0
#define MAX_VOLUME_ALLOWED 500

PlayerState ps = {0};
Font google;
Texture2D playTexture;
Texture2D pauseTexture;
Texture2D ffTexture;
Texture2D bbTexture;
double last_hover_time = -5.0f;
double last_volume_change_time = -5.0f;
double last_audio_change_time = -5.0f;
double last_subtitle_change_time = -5.0f;
char audio_language[512];
char subtitle_language[512];
static float lastClickTime = 0.0f;              // Time of the last click
static const float doubleClickThreshold = 0.3f; // Threshold for double click (in seconds)
static bool show_help_menu = false;             // Help menu visibility

typedef struct
{
    Shader *shaders;
    int shaderCount;
    int capacity;
} ShaderArray;

ShaderArray shaderArray = {0};

char *get_file_title(DecoderState *ds)
{
    while ((ds->tag = av_dict_get(ds->format_ctx->metadata, "", ds->tag, AV_DICT_IGNORE_SUFFIX)))
    {
        if (strcmp(ds->tag->key, "title") == 0)
        {
            return ds->tag->value;
        }
    }
    return "Untitled";
}

void audio_callback(void *buffer, unsigned int frames)
{
    if (!ps.is_playing)
    {
        memset(buffer, 0, frames * sizeof(float) * 2);
        return;
    }
    
    pthread_mutex_lock(&ds.queue_mutex);
    if (av_audio_fifo_size(ds.fifo) >= (int)frames)
    {
        int ret = av_audio_fifo_read(ds.fifo, &buffer, frames);
        if (ret < (int)frames)
        {
            memset((float *)buffer + ret * 2, 0, (frames - ret) * sizeof(float) * 2);
        }
    }
    else
    {
        memset(buffer, 0, frames * sizeof(float) * 2);
    }
    pthread_mutex_unlock(&ds.queue_mutex);
}

void *bundle_load_resource(const char *file_path, size_t *size)
{
    for (size_t i = 0; i < resources_count; ++i)
    {
        if (strcmp(resources[i].file_path, file_path) == 0)
        {
            *size = resources[i].size;
            return &bundle[resources[i].offset];
        }
    }
    return NULL;
}
Font bundle_load_font(const char *file_path, int font_size)
{
    size_t data_size;
    void *data = bundle_load_resource(file_path, &data_size);
    Font output = LoadFontFromMemory(GetFileExtension(file_path), data, data_size, font_size, NULL, 0);
    return output;
}
Texture bundle_load_texture(const char *file_path)
{
    size_t data_size;
    void *data = bundle_load_resource(file_path, &data_size);
    Image image = LoadImageFromMemory(GetFileExtension(file_path), data, data_size);
    Texture output = LoadTextureFromImage(image);
    return output;
}

int GetDisplayWidth(void)
{
    if (IsWindowFullscreen())
    {
        return GetMonitorWidth(GetCurrentMonitor());
    }
    return GetScreenWidth();
}

int GetDisplayHeight(void)
{
    if (IsWindowFullscreen())
    {
        return GetMonitorHeight(GetCurrentMonitor());
    }
    return GetScreenHeight();
}

int player_init(char *filename)
{
    if (decoder_init(filename) < 0)
    {
        fprintf(stderr, "ERROR: Failed to initialize decoder with file: %s\n",
                filename);
        return -1;
    }

    // SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    // InitWindow(800, 600, ps.file_title);
    // SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
    SetWindowPosition(0, 0);
    ps.file_title = get_file_title(&ds);

    printf("---------------------------------------------\n");
    printf("File: %s\n", filename);
    printf("Title: %s\n", ps.file_title);
    printf("---------------------------------------------\n");

    SetWindowTitle(ps.file_title);
    ps.is_playing = true;

    google = bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", FONT_SIZE);
    playTexture = bundle_load_texture("./assets/icons/play.png");
    pauseTexture = bundle_load_texture("./assets/icons/pause.png");
    ffTexture = bundle_load_texture("./assets/icons/ff.png");
    bbTexture = bundle_load_texture("./assets/icons/bb.png");

    InitAudioDevice();
    int sample_rate = ds.audio_codec_ctx->sample_rate;
    int sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 8;
    int channels = 2;
    ps.raylib_audio_stream = LoadAudioStream(sample_rate, sample_size, channels);
    SetAudioStreamCallback(ps.raylib_audio_stream, audio_callback);
    PlayAudioStream(ps.raylib_audio_stream);
    ps.volume = 500;
    SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);

    pthread_mutex_lock(&ds.texture_mutex);
    ps.texture = LoadTextureFromImage(GenImageColor(ds.video_codec_ctx->width, ds.video_codec_ctx->height, BLACK));
    pthread_mutex_unlock(&ds.texture_mutex);

    shaderArray.capacity = 4;
    shaderArray.shaders = malloc(shaderArray.capacity * sizeof(Shader));
    shaderArray.shaderCount = 0;
    return 0;
}

void add_shader(const char *shaderFile)
{
    if (shaderArray.shaderCount >= shaderArray.capacity)
    {
        shaderArray.capacity *= 2;
        shaderArray.shaders =
            realloc(shaderArray.shaders, shaderArray.capacity * sizeof(Shader));
    }
    Shader newShader = LoadShader(0, shaderFile);
    shaderArray.shaders[shaderArray.shaderCount++] = newShader;
}

MouseButtonPressedTimes DetectMouseButtonDoublePressed(int button)
{
    if (IsMouseButtonPressed(button))
    {
        float currentTime = GetTime();
        if ((currentTime - lastClickTime) <= doubleClickThreshold)
        {
            lastClickTime = 0.0f;
            return DOUBLE_CLICK;
        }
        lastClickTime = currentTime;
        return SINGLE_CLICK;
    }
    return BUTTON_CLICK_NONE;
}

void to_timestamp(char *dst, int time)
{
    int hours = time / 3600;
    int minutes = (time % 3600) / 60;
    int seconds = time % 60;
    if (hours > 0)
        sprintf(dst, "%02d:%02d:%02d", hours, minutes, seconds);
    else
        sprintf(dst, "%02d:%02d", minutes, seconds);
}

void TogglePause(void)
{
    if (ps.is_playing)
    {
        ps.is_playing = false;
        pause_decoder();
        PauseAudioStream(ps.raylib_audio_stream);
    }
    else
    {
        ps.is_playing = true;
        ResumeAudioStream(ps.raylib_audio_stream);
        resume_decoder();
    }
}
void player_update(void)
{
    int screenWidth = GetDisplayWidth();
    int screenHeight = GetDisplayHeight();
    float settingHeight = screenHeight * 0.1f;

    double total_runtime = (double)ds.format_ctx->duration / AV_TIME_BASE;

    float dest_width = (float)ds.video_codec_ctx->width;
    float dest_height = (float)ds.video_codec_ctx->height;

    float video_aspect_ratio = dest_width / dest_height;
    float display_aspect_ratio = (float)screenWidth / (float)screenHeight;

    Rectangle dest_rect;
    if (display_aspect_ratio > video_aspect_ratio)
    {
        dest_rect.height = (float)screenHeight;
        dest_rect.width = screenHeight * video_aspect_ratio;
        dest_rect.x = (screenWidth - dest_rect.width) / 2;
        dest_rect.y = 0;
    }
    else
    {
        dest_rect.width = (float)screenWidth;
        dest_rect.height = screenWidth / video_aspect_ratio;
        dest_rect.x = 0;
        dest_rect.y = (screenHeight - dest_rect.height) / 2;
    }

    if (IsKeyPressed(KEY_SPACE))
    {
        TogglePause();
        last_hover_time = GetTime();
    }
    
    // Toggle help menu with ? key (question mark key)
    if (IsKeyPressed(KEY_SLASH) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)))
    {
        show_help_menu = !show_help_menu;
        last_hover_time = GetTime();
    }
    Rectangle setting = {0, (float)screenHeight - settingHeight,
                         (float)screenWidth, settingHeight};
    char elapsed_text[16];
    char total_text[16];

    to_timestamp(elapsed_text, frame_time);
    to_timestamp(total_text, (int)total_runtime);

    Vector2 totalTimeSize = MeasureTextEx(google, total_text, FONT_SIZE / 2, 0);
    Vector2 videoTitleSize = MeasureTextEx(google, ps.file_title, 36, 0);
    float availableWidth = setting.width * 0.95f;

    float starting_left_x = setting.width - availableWidth;

    float seekBarWidth = setting.width * 0.9f;
    float seekBarHeight = setting.height * 0.1f;

    Rectangle seekBar = {starting_left_x, setting.y + setting.height * 0.25f, seekBarWidth, seekBarHeight};
    Rectangle seekBarCurrentPos = {seekBar.x, seekBar.y, seekBar.width * (float)(frame_time / total_runtime), seekBar.height};
    Vector2 elapsedTimePos = {seekBar.x, seekBar.y + seekBar.height * 2};
    Vector2 totalTimePos = {seekBar.x + seekBar.width - (totalTimeSize.x), elapsedTimePos.y};
    Vector2 videoTitlePos = {seekBar.x, seekBar.y - videoTitleSize.y - 8};

    // Vector2 pill = {seekBar.x + seekBar.width * (float)(frame_time / total_runtime), seekBar.y};

    if (IsKeyPressed(KEY_UP) || GetMouseWheelMove() > 0)
    {
        if (ps.volume < MAX_VOLUME_ALLOWED)
        {
            ps.volume += 10;
            TraceLog(LOG_INFO, "Volume: %f", ps.volume);
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);
            last_volume_change_time = GetTime();
        }
    }
    if (IsKeyPressed(KEY_DOWN) || GetMouseWheelMove() < 0)
    {
        if (ps.volume > MIN_VOLUME_ALLOWED)
        {
            ps.volume -= 10;
            TraceLog(LOG_INFO, "Volume: %f", ps.volume);
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);
            last_volume_change_time = GetTime();
        }
    }
    if (IsKeyPressed(KEY_P) && !ps.is_playing)
    {
        if (ExportImage(LoadImageFromTexture(ps.texture), "frame.png"))
        {
            TraceLog(LOG_INFO, "Saved frame in  an image");
        }
    }
    if (IsKeyPressed(KEY_B))
    {
        decoder_change_audio(audio_language);
        last_audio_change_time = GetTime();
    }
    if (IsKeyPressed(KEY_V))
    {
        decoder_change_subtitle(subtitle_language);
        last_subtitle_change_time = GetTime();
    }

    if (IsKeyPressed(KEY_LEFT))
    {
        double seek_time = frame_time - 5.0;
        if (seek_time < 0)
            seek_time = 0;
        if (avformat_seek_file(ds.format_ctx, ds.video_stream_idx, INT64_MIN, seek_time, INT64_MAX, 0) < 0)
        {
            fprintf(stderr, "ERROR: Seek failed during audio change!\n");
        }
        if (avformat_seek_file(ds.format_ctx, ds.audio_stream_idx, INT64_MIN, seek_time, INT64_MAX, 0) < 0)
        {
            fprintf(stderr, "ERROR: Seek failed during audio change!\n");
        }

        avcodec_flush_buffers(ds.video_codec_ctx);
        avcodec_flush_buffers(ds.audio_codec_ctx);
        queue_clear(ds.video_queue);
        av_audio_fifo_reset(ds.fifo);
        
        reset_sync_state();

        frame_time = seek_time;
        last_hover_time = GetTime();
    }

    if (IsKeyPressed(KEY_RIGHT))
    {
        double total_runtime = (double)ds.format_ctx->duration / AV_TIME_BASE;
        double seek_time = frame_time + 5.0;

        if (seek_time > total_runtime)
            seek_time = total_runtime;

        if (avformat_seek_file(ds.format_ctx, ds.video_stream_idx, INT64_MIN, seek_time, INT64_MAX, 0) < 0)
        {
            fprintf(stderr, "ERROR: Seek failed during audio change!\n");
        }
        if (avformat_seek_file(ds.format_ctx, ds.audio_stream_idx, INT64_MIN, seek_time, INT64_MAX, 0) < 0)
        {
            fprintf(stderr, "ERROR: Seek failed during audio change!\n");
        }

        avcodec_flush_buffers(ds.video_codec_ctx);
        avcodec_flush_buffers(ds.audio_codec_ctx);
        queue_clear(ds.video_queue);
        av_audio_fifo_reset(ds.fifo);
        
        reset_sync_state();

        frame_time = seek_time;
        last_hover_time = GetTime();
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (CheckCollisionPointRec(GetMousePosition(), seekBar) ||
            CheckCollisionPointRec(GetMousePosition(), seekBarCurrentPos))
        {
            Vector2 mousePos = GetMousePosition();
            float seekTime = ((mousePos.x - seekBar.x) / seekBar.width) * total_runtime;
            if (seekTime < 0)
                seekTime = 0;
            if (seekTime > total_runtime)
                seekTime = total_runtime;

            int flags = 0;
            if (seekTime < frame_time)
                flags = AVSEEK_FLAG_BACKWARD;

            printf("Seeking to %.2f seconds\n", seekTime);
            int64_t seek_target = (int64_t)(seekTime / av_q2d(ds.video_stream->time_base));
            if (avformat_seek_file(ds.format_ctx, ds.video_stream_idx, INT64_MIN, seek_target, INT64_MAX, flags) < 0)
            {
                printf("Seek error!\n");
            }
            avcodec_flush_buffers(ds.video_codec_ctx);
            avcodec_flush_buffers(ds.audio_codec_ctx);
            queue_clear(ds.video_queue);
            av_audio_fifo_reset(ds.fifo);
            
            reset_sync_state();
            
            frame_time = seekTime;
        }
        else
        {
            TogglePause();
            last_hover_time = GetTime();
        }
    }
    if (IsFileDropped())
    {
        FilePathList droppedFiles = LoadDroppedFiles();
        for (size_t i = 0; i < droppedFiles.count; i++)
        {
            if (GetFileExtension(droppedFiles.paths[i]) &&
                strcmp(GetFileExtension(droppedFiles.paths[i]), ".fs") ==
                    0)
            {
                add_shader(droppedFiles.paths[i]);
            }
        }
        UnloadDroppedFiles(droppedFiles);
    }

    if (IsKeyPressed(KEY_U))
    {
        for (int i = 0; i < shaderArray.shaderCount; i++)
        {
            UnloadShader(shaderArray.shaders[i]);
        }
        shaderArray.shaderCount = 0;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    if (shaderArray.shaderCount > 0)
    {
        BeginShaderMode(shaderArray.shaders[0]); // Start with first shader
        for (int i = 1; i < shaderArray.shaderCount; i++)
        {
            BeginShaderMode(shaderArray.shaders[i]); // Stack additional shaders
        }
    }
    pthread_mutex_lock(&ds.texture_mutex);
    UpdateTexture(ps.texture, ds.rgba_frame_buffer);
    pthread_mutex_unlock(&ds.texture_mutex);
    DrawTexturePro(ps.texture,
                   (Rectangle){0, 0, (float)ds.video_codec_ctx->width,
                               (float)ds.video_codec_ctx->height},
                   dest_rect, Vector2Zero(), 0, WHITE);

    for (int i = 0; i < shaderArray.shaderCount; i++)
    {
        EndShaderMode();
    }

    if (GetMousePosition().y > screenHeight - settingHeight)
        last_hover_time = GetTime();
    if (GetTime() - last_hover_time < 5.0f)
    {
        double alpha = 1 - (GetTime() - last_hover_time);
        DrawRectangleRec(seekBar, Fade(GetColor(0xB7B7B7FF), alpha));
        DrawRectangleRec(seekBarCurrentPos, Fade(DARKBLUE, alpha));
        DrawCircleSector((Vector2){seekBarCurrentPos.x + seekBarCurrentPos.width, seekBarCurrentPos.y + seekBarCurrentPos.height / 2}, seekBarCurrentPos.height / 2, 270, 360 + 90, 50, Fade(DARKBLUE, alpha));
        DrawCircleSector((Vector2){seekBar.x, seekBar.y + seekBar.height / 2}, seekBar.height / 2, 90, 270, 50, Fade(GetColor(0xB7B7B7FF), alpha));
        DrawCircleSector((Vector2){seekBar.x, seekBar.y + seekBar.height / 2}, seekBar.height / 2, 90, 270, 50, Fade(DARKBLUE, alpha));
        DrawCircleSector((Vector2){seekBar.x + seekBar.width, seekBar.y + seekBar.height / 2}, seekBar.height / 2, 270, 360 + 90, 50, Fade(GetColor(0xB7B7B7FF), alpha));

        DrawTextEx(google, elapsed_text, elapsedTimePos, FONT_SIZE / 1.5, 0, Fade(RAYWHITE, alpha));
        DrawTextEx(google, total_text, totalTimePos, FONT_SIZE / 1.5, 0, Fade(RAYWHITE, alpha));
        DrawTextEx(google, ps.file_title, videoTitlePos, FONT_SIZE, 0, Fade(RAYWHITE, alpha));

        // DrawCircleV((Vector2){screenWidth / 2, screenHeight / 2}, 48, Fade(RAYWHITE, alpha));

        DrawTexturePro(ps.is_playing ? pauseTexture : playTexture,
                       (Rectangle){0, 0, playTexture.width, playTexture.height},
                       (Rectangle){screenWidth / 2 - playTexture.width / 2, screenHeight / 2 - playTexture.height / 2, playTexture.width, playTexture.height},
                       (Vector2){0, 0}, 0.0f,
                       Fade(DARKBLUE, alpha));
    }
    if (GetTime() - last_volume_change_time < 3.0f)
    {
        int vol_height = 400;
        int vol_width = 20;
        float filled_height = vol_height * ps.volume / MAX_VOLUME_ALLOWED;
        float filled_y = (GetScreenHeight() / 2.0f) + (vol_height / 2.0f) - filled_height;
        Rectangle vol_inner_rect = {
            GetScreenWidth() - 2 * vol_width,
            filled_y,
            vol_width,
            filled_height};

        DrawRectangleLines(
            GetScreenWidth() - 2 * vol_width,
            (GetScreenHeight() / 2.0f) - vol_height / 2.0f,
            vol_width,
            vol_height,
            ORANGE);
        DrawRectangleRec(vol_inner_rect, Fade(ORANGE, 0.8f));
        Vector2 textSize = MeasureTextEx(google, TextFormat("Volume: %d%%", (int)ps.volume), FONT_SIZE, 0);
        Vector2 textPos = {GetScreenWidth() - textSize.x - 10, 10 + textSize.y};
        DrawTextEx(google, TextFormat("Volume: %d%%", (int)ps.volume), textPos, FONT_SIZE, 0, RAYWHITE);
    }

    if (GetTime() - last_audio_change_time < 3.0f)
    {
        Vector2 textSize = MeasureTextEx(google, TextFormat("Audio: %s", audio_language), FONT_SIZE, 0);
        Vector2 textPos = {GetScreenWidth() - textSize.x - 10, 10 + textSize.y};
        DrawTextEx(google, TextFormat("Audio: %s", audio_language), textPos, FONT_SIZE, 0, RAYWHITE);
    }
    if (GetTime() - last_subtitle_change_time < 3.0f)
    {
        Vector2 textSize = MeasureTextEx(google, TextFormat("Subtitle: %s", subtitle_language), FONT_SIZE, 0);
        Vector2 textPos = {GetScreenWidth() - textSize.x - 10, 10 + textSize.y};
        DrawTextEx(google, TextFormat("Subtitle: %s", subtitle_language), textPos, FONT_SIZE, 0, RAYWHITE);
    }

    Vector2 a = MeasureTextEx(google, ds.current_subtitle, FONT_SIZE * 1.25, 0);
    DrawTextEx(google, ds.current_subtitle,
               (Vector2){screenWidth / 2 - a.x / 2, screenHeight - settingHeight + settingHeight * 0.15f},
               FONT_SIZE * 1.25, 0, WHITE);

    // Draw help menu if visible
    if (show_help_menu)
    {
        // Semi-transparent overlay background
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.8f));
        
        // Help menu title
        const char *title = "KEYBOARD CONTROLS";
        Vector2 titleSize = MeasureTextEx(google, title, FONT_SIZE * 1.5, 0);
        Vector2 titlePos = {screenWidth / 2 - titleSize.x / 2, 50};
        DrawTextEx(google, title, titlePos, FONT_SIZE * 1.5, 0, WHITE);
        
        // Help menu content
        const char *help_text[] = {
            "SPACE           - Play/Pause",
            "LEFT/RIGHT      - Seek -5s/+5s",
            "UP/DOWN         - Volume +/-",
            "Mouse Wheel     - Volume +/-",
            "B               - Change Audio Track",
            "V               - Change Subtitle Track",
            "P               - Save Screenshot (when paused)",
            "U               - Unload Shaders",
            "?               - Toggle this Help Menu",
            "ESC             - Close Player",
            "",
            "MOUSE CONTROLS:",
            "Click Seekbar   - Seek to Position",
            "Click Video     - Play/Pause",
            "Drag & Drop     - Load Shader (.fs files)"
        };
        
        int help_lines = sizeof(help_text) / sizeof(help_text[0]);
        float line_height = FONT_SIZE * 0.8f + 5;
        float start_y = titlePos.y + titleSize.y + 40;
        
        for (int i = 0; i < help_lines; i++)
        {
            Vector2 pos = {100, start_y + i * line_height};
            Color text_color = (strlen(help_text[i]) == 0) ? BLANK : 
                              (strstr(help_text[i], "MOUSE CONTROLS") != NULL) ? YELLOW : WHITE;
            if (text_color.a > 0)
                DrawTextEx(google, help_text[i], pos, FONT_SIZE * 0.8f, 0, text_color);
        }
        
        // Instructions at bottom
        const char *close_text = "Press ? again to close this menu";
        Vector2 closeSize = MeasureTextEx(google, close_text, FONT_SIZE * 0.7f, 0);
        Vector2 closePos = {screenWidth / 2 - closeSize.x / 2, screenHeight - 50};
        DrawTextEx(google, close_text, closePos, FONT_SIZE * 0.7f, 0, GRAY);
    }
    
    EndDrawing();
}

void player_close(void)
{
    decoder_stop();
    for (int i = 0; i < shaderArray.shaderCount; i++)
    {
        UnloadShader(shaderArray.shaders[i]);
    }
    free(shaderArray.shaders);

    UnloadTexture(ps.texture);
    UnloadAudioStream(ps.raylib_audio_stream);
    CloseAudioDevice();

    UnloadFont(google);
    UnloadTexture(playTexture);
    UnloadTexture(pauseTexture);
    UnloadTexture(ffTexture);
    UnloadTexture(bbTexture);

    CloseWindow();
}
