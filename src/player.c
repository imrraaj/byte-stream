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
#define MAX_VOLUME_ALLOWED 250

PlayerState ps = {0};
bool isPlaying = true;
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
Font bundle_load_font(const char *file_path)
{
    size_t data_size;
    void *data = bundle_load_resource(file_path, &data_size);
    Font output = LoadFontFromMemory(GetFileExtension(file_path), data, data_size, FONT_SIZE, NULL, 0);
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
    // SetTargetFPS(3);
    // SetGesturesEnabled(GESTURE_TAP | GESTURE_DOUBLETAP);

    google = bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf");
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
    ps.volume = 100;
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
    if (display_aspect_ratio < video_aspect_ratio)
    {
        dest_rect.height = (float)screenHeight;
        dest_rect.width = screenHeight * video_aspect_ratio;
        dest_rect.x = -(dest_rect.width - screenWidth) / 2;
        dest_rect.y = 0;
    }
    else
    {
        dest_rect.width = (float)screenWidth;
        dest_rect.height = screenWidth / video_aspect_ratio;
        dest_rect.x = 0;
        dest_rect.y = -(dest_rect.height - screenHeight) / 2;
    }

    if (IsKeyPressed(KEY_SPACE))
    {
        isPlaying = !isPlaying;
        last_hover_time = GetTime();
    }
    if (isPlaying)
    {
        isPlaying ? ResumeAudioStream(ps.raylib_audio_stream) : PauseAudioStream(ps.raylib_audio_stream);
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

    if (IsKeyPressed(KEY_UP))
    {
        if (ps.volume < MAX_VOLUME_ALLOWED)
        {
            ps.volume += 10;
            TraceLog(LOG_INFO, "Volume: %f", ps.volume);
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);
            last_volume_change_time = GetTime();
        }
    }
    if (IsKeyPressed(KEY_DOWN))
    {
        if (ps.volume > MIN_VOLUME_ALLOWED)
        {
            ps.volume -= 10;
            TraceLog(LOG_INFO, "Volume: %f", ps.volume);
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);
            last_volume_change_time = GetTime();
        }
    }
    if (IsKeyPressed(KEY_P) && !isPlaying)
    {
        if (ExportImage(LoadImageFromTexture(ps.texture), "frame.png"))
        {
            TraceLog(LOG_INFO, "Saved frame in  an image");
        }
    }
    if (IsKeyPressed(KEY_B))
    {
        // decoder_change_audio(audio_language);
        last_audio_change_time = GetTime();
    }
    if (IsKeyPressed(KEY_V))
    {
        // decoder_change_subtitle(subtitle_language);
        last_subtitle_change_time = GetTime();
    }

    if (IsKeyPressed(KEY_LEFT))
    {
        double seek_time = frame_time - 5.0;
        if (seek_time < 0)
            seek_time = 0;
        printf("Seeking backward to %.2f seconds\n", seek_time);
        int64_t seek_target = (int64_t)(seek_time / av_q2d(ds.video_stream->time_base));
        if (avformat_seek_file(
                ds.format_ctx,
                ds.video_stream_idx,
                INT64_MIN,
                seek_target,
                INT64_MAX,
                AVSEEK_FLAG_BACKWARD) < 0)
        {
            printf("Seek error!\n");
        }

        avcodec_flush_buffers(ds.video_codec_ctx);
        av_audio_fifo_reset(ds.fifo);
        frame_time = seek_time;
        last_hover_time = GetTime();
    }

    if (IsKeyPressed(KEY_RIGHT))
    {
        double total_runtime = (double)ds.format_ctx->duration / AV_TIME_BASE;
        double seek_time = frame_time + 5.0;

        if (seek_time > total_runtime)
            seek_time = total_runtime;

        printf("Seeking forward to %.2f seconds\n", seek_time);
        int64_t seek_target = (int64_t)(seek_time / av_q2d(ds.video_stream->time_base));
        if (avformat_seek_file(ds.format_ctx, ds.video_stream_idx, INT64_MIN, seek_target, INT64_MAX, 0) < 0)
        {
            printf("Seek error!\n");
        }
        avcodec_flush_buffers(ds.video_codec_ctx);
        av_audio_fifo_reset(ds.fifo);
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
            av_audio_fifo_reset(ds.fifo);
            frame_time = seekTime;
        }
        else
        {
            isPlaying = !isPlaying;
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
    DrawTexturePro(ps.texture,
                   (Rectangle){0, 0, (float)ds.video_codec_ctx->width,
                               (float)ds.video_codec_ctx->height},
                   dest_rect, Vector2Zero(), 0, WHITE);
    pthread_mutex_unlock(&ds.texture_mutex);

    // for (int i = 0; i < shaderArray.shaderCount; i++)
    // {
    //     EndShaderMode();
    // }

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

        DrawTexturePro(isPlaying ? pauseTexture : playTexture,
                       (Rectangle){0, 0, playTexture.width, playTexture.height},
                       (Rectangle){screenWidth / 2 - playTexture.width / 2, screenHeight / 2 - playTexture.height / 2, playTexture.width, playTexture.height},
                       (Vector2){0, 0}, 0.0f,
                       Fade(DARKBLUE, alpha));
    }
    if (GetTime() - last_volume_change_time < 3.0f)
    {
        int vol_height = 400;
        int vol_width = 20;
        Rectangle vol_inner_rect = {GetScreenWidth() - 2 * vol_width, (GetScreenHeight() / 2.0f) - vol_height / 2.0, vol_width, vol_height * ps.volume / 250};
        DrawRectangleLines(GetScreenWidth() - 2 * vol_width, (GetScreenHeight() / 2.0f) - vol_height / 2.0, vol_width, vol_height, BLACK);
        DrawRectangleRec(vol_inner_rect, BLACK);
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
