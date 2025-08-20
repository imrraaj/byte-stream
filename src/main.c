#include "decoder.h"
#include "player.h"
#include "tinyfiledialogs.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define BYTESTREAM_VERSION "0.1.0"
bool isFileSelected = false;
char errorMsg[256] = {0};

// Font management
Font *get_best_font(Font *fonts, int *font_sizes, int count, int target_size) {
    int best_idx = 0;
    int smallest_diff = abs(font_sizes[0] - target_size);
    
    for (int i = 1; i < count; i++) {
        int diff = abs(font_sizes[i] - target_size);
        if (diff < smallest_diff) {
            smallest_diff = diff;
            best_idx = i;
        }
    }
    return &fonts[best_idx];
}

int main(int argc, char **argv)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "Byte Stream Player");
    SetTargetFPS(60);
    
    // Enable text anti-aliasing
    SetTextLineSpacing(0);

    Texture2D logo_texture = bundle_load_texture("./assets/logos/bytestream-256.png");
    Image logo = LoadImageFromTexture(logo_texture);
    SetWindowIcon(logo);

    // Load multiple font sizes for sharp rendering
    Font fonts[8] = {
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 12),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 16),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 20),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 24),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 32),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 48),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 64),
        bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 72)
    };
    int font_sizes[] = {12, 16, 20, 24, 32, 48, 64, 72};

    if (argc > 1)
    {
        if (player_init(argv[1]) < 0)
        {
            snprintf(errorMsg, sizeof(errorMsg), "Failed to initialize player with file: %s", argv[1]);
        }
        else
        {
            isFileSelected = true;
        }
    }

    while (!WindowShouldClose())
    {
        if (isFileSelected)
        {
            player_update();
        }
        else
        {
            int screenWidth = GetScreenWidth();
            int screenHeight = GetScreenHeight();
            int padding = 30;
            int logoX = (screenWidth - logo_texture.width) / 2;
            int logoY = (screenHeight - logo_texture.height) / 2 - 80;

            char *select_text = "Byte Stream - v" BYTESTREAM_VERSION;
            Font *title_font = get_best_font(fonts, font_sizes, 8, 48);
            // Use exact font size, no scaling
            float title_size = 48.0f;
            Vector2 textSize1 = MeasureTextEx(*title_font, select_text, title_size, 1.0f);
            Vector2 text1 = {(screenWidth - textSize1.x) / 2, logoY + logo_texture.height};

            Rectangle buttonRect = {
                (screenWidth - 200) / 2,
                text1.y + textSize1.y + padding,
                200,
                40,
            };

            Color buttonColor = DARKGRAY;
            if (CheckCollisionPointRec(GetMousePosition(), buttonRect))
            {
                buttonColor = DARKGREEN;
                SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    char *file_name = tinyfd_openFileDialog("Please select a video file to play", ".", 1, NULL, NULL, 1);
                    if (file_name)
                    {
                        if (player_init(file_name) < 0)
                        {
                            snprintf(errorMsg, sizeof(errorMsg), "Failed to initialize player with file: %s", file_name);
                        }
                        else
                        {
                            isFileSelected = true;
                            errorMsg[0] = '\0';
                        }
                    }
                }
            }
            else
            {
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            }

            BeginDrawing();
            ClearBackground(GetColor(0x181818FF));
            DrawTexture(logo_texture, logoX, logoY, WHITE);
            DrawTextEx(*title_font, select_text, text1, title_size, 1.0f, WHITE);
            DrawRectangleRec(buttonRect, buttonColor);
            Font *button_font = get_best_font(fonts, font_sizes, 8, 24);
            float button_size = 24.0f;
            Vector2 buttonTextSize = MeasureTextEx(*button_font, "Select Video", button_size, 1.0f);
            DrawTextEx(*button_font, "Select Video",
                       (Vector2){
                           buttonRect.x + (buttonRect.width - buttonTextSize.x) / 2,
                           buttonRect.y + (buttonRect.height - buttonTextSize.y) / 2},
                       button_size, 1.0f, WHITE);

            if (errorMsg[0] != '\0')
            {
                Font *error_font = get_best_font(fonts, font_sizes, 8, 20);
                float error_size = 20.0f;
                Vector2 errorSize = MeasureTextEx(*error_font, errorMsg, error_size, 1.0f);
                DrawTextEx(*error_font, errorMsg,
                           (Vector2){
                               (screenWidth - errorSize.x) / 2,
                               buttonRect.y + buttonRect.height + padding},
                           error_size, 1.0f, RED);
            }
            EndDrawing();
        }
    }

    UnloadImage(logo);
    if (isFileSelected)
        player_close();
    return 0;
}
