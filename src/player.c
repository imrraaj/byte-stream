#include "player.h"
#include "decoder.h"
#include "raylib.h"
#include "raymath.h"
#include "bundle.h"
#include "application.h"
#include <math.h>
#include <stdio.h>

#define FONT_SIZE 36
#define ICON_SIZE 32 / 1.25
#define BUTTON_RADIUS ICON_SIZE * 1.5f
#define MIN_VOLUME_ALLOWED 0
#define MAX_VOLUME_ALLOWED 500
typedef struct
{
    Shader *shaders;
    int count;
    int capacity;
} ShaderArray;

PlayerState ps = {0};
Texture2D playTexture;
Texture2D pauseTexture;
Texture2D ffTexture;
Texture2D bbTexture;
Texture2D volumeTexture;
Texture2D muteTexture;
double last_hover_time = -5.0f;
double last_volume_change_time = -5.0f;
double last_audio_change_time = -5.0f;
double last_subtitle_change_time = -5.0f;
char audio_language[512];
char subtitle_language[512];
static float lastClickTime = 0.0f;              // Time of the last click
static const float doubleClickThreshold = 0.3f; // Threshold for double click (in seconds)
static bool show_help_menu = false;             // Help menu visibility
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
    ps.file_title = decoder_get_metadata(&ds, "title");
    if (!ps.file_title || strlen(ps.file_title) == 0)
        ps.file_title = "Untitled";
    SetWindowTitle(ps.file_title);
    ps.is_playing = true;

    ps.volume = 100.0f;
    playTexture = bundle_load_texture("./assets/icons/play.png");
    pauseTexture = bundle_load_texture("./assets/icons/pause.png");
    ffTexture = bundle_load_texture("./assets/icons/ff.png");
    bbTexture = bundle_load_texture("./assets/icons/bb.png");
    volumeTexture = bundle_load_texture("./assets/icons/volume.png");
    muteTexture = bundle_load_texture("./assets/icons/mute.png");

    InitAudioDevice();
    int sample_rate = ds.audio_codec_ctx->sample_rate;
    int sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 8;
    int channels = 2;
    ps.raylib_audio_stream = LoadAudioStream(sample_rate, sample_size, channels);
    SetAudioStreamCallback(ps.raylib_audio_stream, audio_callback);
    PlayAudioStream(ps.raylib_audio_stream);
    ps.volume = 500;
    ps.is_muted = false;
    SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);

    pthread_mutex_lock(&ds.texture_mutex);
    ps.texture = LoadTextureFromImage(GenImageColor(ds.video_codec_ctx->width, ds.video_codec_ctx->height, BLACK));
    pthread_mutex_unlock(&ds.texture_mutex);

    shaderArray.capacity = 4;
    shaderArray.shaders = malloc(shaderArray.capacity * sizeof(Shader));
    shaderArray.count = 0;
    return 0;
}

void add_shader(const char *shaderFile)
{
    if (shaderArray.count >= shaderArray.capacity)
    {
        shaderArray.capacity *= 2;
        shaderArray.shaders =
            realloc(shaderArray.shaders, shaderArray.capacity * sizeof(Shader));
    }
    Shader newShader = LoadShader(0, shaderFile);
    shaderArray.shaders[shaderArray.count++] = newShader;
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
    float settingHeight = fmaxf(screenHeight * 0.15f, 100.0f); // Minimum 100px height for controls

    double total_runtime = (double)ds.format_ctx->duration / AV_TIME_BASE;

    float dest_width = (float)ds.video_codec_ctx->width;
    float dest_height = (float)ds.video_codec_ctx->height;

    float video_aspect_ratio = dest_width / dest_height;
    float display_aspect_ratio = (float)screenWidth / (float)screenHeight;

    Rectangle dest_rect;
    
    // Video takes full screen, respecting aspect ratio
    if (display_aspect_ratio > video_aspect_ratio)
    {
        // Screen is wider than video - fit video height to screen, center horizontally
        dest_rect.height = (float)screenHeight;
        dest_rect.width = screenHeight * video_aspect_ratio;
        dest_rect.x = (screenWidth - dest_rect.width) / 2;
        dest_rect.y = 0;
    }
    else
    {
        // Screen is taller than video - fit video width to screen, center vertically
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



    // Controls area at bottom of screen as overlay
    Rectangle setting = {0, (float)screenHeight - settingHeight,
                         (float)screenWidth, settingHeight};
    char elapsed_text[16];
    char total_text[16];
    char time_text[32];

    to_timestamp(elapsed_text, frame_time);
    to_timestamp(total_text, (int)total_runtime);
    sprintf(time_text, "%s / %s", elapsed_text, total_text);

    // Scale UI elements based on screen size
    float scale_factor = fminf(screenWidth / 1200.0f, screenHeight / 800.0f);
    scale_factor = fmaxf(scale_factor, 0.5f); // Minimum scale to keep readable
    scale_factor = fminf(scale_factor, 1.5f); // Maximum scale to prevent oversized UI
    
    int time_fontsize = (int)(20 * scale_factor);
    Font time_font = get_best_font(app.fonts, time_fontsize);
    int title_fontsize = (int)(20 * scale_factor);
    Font title_font = get_best_font(app.fonts, title_fontsize);
    Vector2 timeTextSize = MeasureTextEx(time_font, time_text, time_fontsize, 0);
    Vector2 videoTitleSize = MeasureTextEx(title_font, ps.file_title, title_fontsize, 0);
    
    float margin = 10 * scale_factor;
    float seekBarWidth = setting.width - 2 * margin;
    float seekBarHeight = 8 * scale_factor;
    float controlsRowHeight = 32 * scale_factor;
    float controlSpacing = 10 * scale_factor;
    float iconSize = ICON_SIZE * scale_factor;
    
    // Calculate exact space needed for controls
    float topPadding = 16 * scale_factor;
    float seekBarToControlsGap = 10 * scale_factor;
    float bottomPadding = 8 * scale_factor;

    // Calculate total height needed
    float totalControlsHeight = topPadding + seekBarHeight + seekBarToControlsGap + controlsRowHeight + bottomPadding;
    
    // Position controls at absolute bottom
    float controlsStartY = screenHeight - totalControlsHeight;
    float seekBarY = controlsStartY + topPadding;
    Rectangle seekBar = {margin, seekBarY, seekBarWidth, seekBarHeight};
    Rectangle seekBarCurrentPos = {seekBar.x, seekBar.y, seekBar.width * (float)(frame_time / total_runtime), seekBar.height};
    
    // Controls row positioned below seekbar
    float controlsY = seekBar.y + seekBar.height + seekBarToControlsGap;
    float currentX = margin;
    
    // Play/Pause button
    Rectangle playPauseButton = {currentX, controlsY, iconSize, iconSize};
    currentX += iconSize + controlSpacing;
    
    // Next button  
    Rectangle nextButton = {currentX, controlsY, iconSize, iconSize};
    currentX += iconSize + controlSpacing;
    
    // Volume button
    Rectangle volumeButton = {currentX, controlsY, iconSize, iconSize};
    currentX += iconSize + controlSpacing;
    
    // Time display
    Vector2 timePos = {currentX, controlsY + (iconSize - time_fontsize) / 2};
    currentX += timeTextSize.x + controlSpacing;
    
    // Video title (right-aligned or remaining space)
    float remainingWidth = setting.width - currentX - margin;
    Vector2 titlePos = {currentX, controlsY + (iconSize - title_fontsize) / 2};

    // Vector2 pill = {seekBar.x + seekBar.width * (float)(frame_time / total_runtime), seekBar.y};

    if (CheckCollisionPointRec(GetMousePosition(), seekBar))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    if (IsKeyPressed(KEY_UP) || GetMouseWheelMove() > 0)
    {
        if (ps.volume < MAX_VOLUME_ALLOWED && !ps.is_muted)
        {
            ps.volume += 10;
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);
            last_volume_change_time = GetTime();
        }
    }
    if (IsKeyPressed(KEY_DOWN) || GetMouseWheelMove() < 0)
    {
        if (ps.volume > MIN_VOLUME_ALLOWED && !ps.is_muted)
        {
            ps.volume -= 10;
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.volume / 100);
            last_volume_change_time = GetTime();
        }
    }
    if (IsKeyPressed(KEY_M))
    {
        ps.is_muted = !ps.is_muted;
        SetAudioStreamVolume(ps.raylib_audio_stream, ps.is_muted ? 0 : ps.volume / 100);
        last_volume_change_time = GetTime();
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
        Vector2 mousePos = GetMousePosition();
        
        if (CheckCollisionPointRec(mousePos, seekBar) ||
            CheckCollisionPointRec(mousePos, seekBarCurrentPos))
        {
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
            last_hover_time = GetTime();
        }
        else if (CheckCollisionPointRec(mousePos, playPauseButton))
        {
            TogglePause();
            last_hover_time = GetTime();
        }
        else if (CheckCollisionPointRec(mousePos, volumeButton))
        {
            ps.is_muted = !ps.is_muted;
            SetAudioStreamVolume(ps.raylib_audio_stream, ps.is_muted ? 0 : ps.volume / 100);
            last_volume_change_time = GetTime();
            last_hover_time = GetTime();
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
        for (int i = 0; i < shaderArray.count; i++)
        {
            UnloadShader(shaderArray.shaders[i]);
        }
        shaderArray.count = 0;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    if (shaderArray.count > 0)
    {
        BeginShaderMode(shaderArray.shaders[0]); // Start with first shader
        for (int i = 1; i < shaderArray.count; i++)
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

    for (int i = 0; i < shaderArray.count; i++)
    {
        EndShaderMode();
    }

    // Show controls when hovering over calculated control area OR when window is very small
    bool shouldShowControls = (GetMousePosition().y > controlsStartY) || 
                             (screenWidth < 600 || screenHeight < 400);
    
    if (shouldShowControls)
        last_hover_time = GetTime();
    if (GetTime() - last_hover_time < 3.0f || shouldShowControls)
    {
        double alpha = shouldShowControls ? 1.0 : (1.0 - (GetTime() - last_hover_time) / 3.0);
        if (alpha < 0) alpha = 0;
        if (alpha > 1) alpha = 1;
        
        // Draw glass-like background for exact controls area needed
        Rectangle controlsBackground = {
            0, 
            controlsStartY,
            (float)screenWidth,
            totalControlsHeight
        };
        DrawRectangleRounded(controlsBackground, 0.1f, 10, GetColor(0x181920C0));
        // Draw seekbar
        DrawRectangleRounded(seekBar, 0.5f, 10, Fade(GetColor(0x444444FF), alpha));
        DrawRectangleRounded(seekBarCurrentPos, 0.5f, 10, Fade(ACCENT_COLOR, alpha));
        
        // Draw play/pause button
        DrawTexturePro(ps.is_playing ? pauseTexture : playTexture,
                       (Rectangle){0, 0, playTexture.width, playTexture.height},
                       playPauseButton,
                       (Vector2){0, 0}, 0.0f,
                       Fade(ACCENT_COLOR, alpha));
        
        // Draw next button
        DrawTexturePro(ffTexture,
                       (Rectangle){0, 0, ffTexture.width, ffTexture.height},
                       nextButton,
                       (Vector2){0, 0}, 0.0f,
                       Fade(RAYWHITE, alpha));
        // Draw volume button (mute/unmute)
        DrawTexturePro(ps.is_muted ? muteTexture : volumeTexture,
                       (Rectangle){0, 0, volumeTexture.width, volumeTexture.height},
                       volumeButton,
                       (Vector2){0, 0}, 0.0f,
                       Fade(RAYWHITE, alpha));
        
        // Draw time display
        DrawTextEx(time_font, time_text, timePos, time_fontsize, 0, Fade(RAYWHITE, alpha));
        
        // Draw video title (truncate if too long)
        const char *displayTitle = ps.file_title;
        char truncatedTitle[256];
        if (videoTitleSize.x > remainingWidth && remainingWidth > 50) {
            int maxChars = (remainingWidth - 20) / (title_fontsize * 0.6f); // rough estimate
            if (maxChars > 0 && maxChars < 253) {
                strncpy(truncatedTitle, ps.file_title, maxChars);
                truncatedTitle[maxChars] = '.';
                truncatedTitle[maxChars + 1] = '.';
                truncatedTitle[maxChars + 2] = '.';
                truncatedTitle[maxChars + 3] = '\0';
                displayTitle = truncatedTitle;
            }
        }
        DrawTextEx(title_font, displayTitle, titlePos, title_fontsize, 0, Fade(RAYWHITE, alpha));
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
        Font volume_font = get_best_font(app.fonts, FONT_SIZE);
        Vector2 textSize = MeasureTextEx(volume_font, TextFormat("Volume: %d%%", (int)ps.volume), FONT_SIZE, 0);
        Vector2 textPos = {GetScreenWidth() - textSize.x - 10, 10 + textSize.y};
        DrawTextEx(volume_font, TextFormat("Volume: %d%%", (int)ps.volume), textPos, FONT_SIZE, 0, RAYWHITE);
    }

    if (GetTime() - last_audio_change_time < 3.0f)
    {
        Font audio_font = get_best_font(app.fonts, FONT_SIZE);
        Vector2 textSize = MeasureTextEx(audio_font, TextFormat("Audio: %s", audio_language), FONT_SIZE, 0);
        Vector2 textPos = {GetScreenWidth() - textSize.x - 10, 10 + textSize.y};
        DrawTextEx(audio_font, TextFormat("Audio: %s", audio_language), textPos, FONT_SIZE, 0, RAYWHITE);
    }
    if (GetTime() - last_subtitle_change_time < 3.0f)
    {
        Font subtitle_font = get_best_font(app.fonts, FONT_SIZE);
        Vector2 textSize = MeasureTextEx(subtitle_font, TextFormat("Subtitle: %s", subtitle_language), FONT_SIZE, 0);
        Vector2 textPos = {GetScreenWidth() - textSize.x - 10, 10 + textSize.y};
        DrawTextEx(subtitle_font, TextFormat("Subtitle: %s", subtitle_language), textPos, FONT_SIZE, 0, RAYWHITE);
    }

    // Display subtitle if one should be visible at current time
    const char *current_subtitle = get_current_subtitle((double)frame_time);
    if (current_subtitle && current_subtitle[0] != '\0')
    {
        Font current_subtitle_font = get_best_font(app.fonts, FONT_SIZE * 1.25);
        Vector2 a = MeasureTextEx(current_subtitle_font, current_subtitle, FONT_SIZE * 1.25, 0);
        DrawTextEx(current_subtitle_font, current_subtitle,
                   (Vector2){screenWidth / 2 - a.x / 2, screenHeight - settingHeight + settingHeight * 0.15f},
                   FONT_SIZE * 1.25, 0, WHITE);
    }

    // Draw help menu if visible
    if (show_help_menu)
    {
        // Semi-transparent overlay background
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.8f));

        // Help menu title
        const char *title = "KEYBOARD CONTROLS";
        int help_fontsize = 24;
        Font help_title_font = get_best_font(app.fonts, help_fontsize);
        Vector2 titleSize = MeasureTextEx(help_title_font, title, help_fontsize, 0);
        Vector2 titlePos = {screenWidth / 2 - titleSize.x / 2, 50};
        DrawTextEx(help_title_font, title, titlePos, help_fontsize, 0, WHITE);

        // Help menu content
        const char *help_text[] = {
            "SPACE           - Play/Pause",
            "LEFT/RIGHT      - Seek -5s/+5s",
            "UP/DOWN         - Volume +/-",
            "Mouse Wheel     - Volume +/-",
            "M               - Mute/Unmute",
            "B               - Change Audio Track",
            "V               - Change Subtitle Track",
            "P               - Save Screenshot (when paused)",
            "U               - Unload Shaders",
            "?               - Toggle this Help Menu",
            "ESC             - Close Player",
            "",
            "MOUSE CONTROLS:",
            "Click Seekbar   - Seek to Position",
            "Click Play/Pause- Play/Pause",
            "Click Volume    - Mute/Unmute",
            "Click Video     - Play/Pause",
            "Drag & Drop     - Load Shader (.fs files)"};

        int help_lines = sizeof(help_text) / sizeof(help_text[0]);
        float line_height = help_fontsize + 5;
        float start_y = titlePos.y + titleSize.y + 40;

        Font help_content_font = get_best_font(app.fonts, help_fontsize);
        for (int i = 0; i < help_lines; i++)
        {
            Vector2 pos = {100, start_y + i * line_height};
            Color text_color = WHITE;
            if (text_color.a > 0)
                DrawTextEx(help_content_font, help_text[i], pos, help_fontsize * 0.8f, 0, text_color);
        }

        // Instructions at bottom

        const char *close_text = "Press ? again to close this menu";
        Font help_close_font = get_best_font(app.fonts, help_fontsize * 0.7f);
        Vector2 closeSize = MeasureTextEx(help_close_font, close_text, help_fontsize * 0.7f, 0);
        Vector2 closePos = {screenWidth / 2 - closeSize.x / 2, screenHeight - 50};
        DrawTextEx(help_close_font, close_text, closePos, help_fontsize * 0.7f, 0, GRAY);
    }

    EndDrawing();
}

void player_close(void)
{
    decoder_stop();
    for (int i = 0; i < shaderArray.count; i++)
    {
        UnloadShader(shaderArray.shaders[i]);
    }
    free(shaderArray.shaders);

    UnloadTexture(ps.texture);
    UnloadAudioStream(ps.raylib_audio_stream);
    CloseAudioDevice();
    UnloadTexture(playTexture);
    UnloadTexture(pauseTexture);
    UnloadTexture(ffTexture);
    UnloadTexture(bbTexture);
    UnloadTexture(volumeTexture);
    UnloadTexture(muteTexture);
}
