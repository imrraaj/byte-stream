#include "decoder.h"
#include "player.h"
#include "application.h"
#include "tinyfiledialogs.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#define BYTESTREAM_VERSION "0.1.0"

Application app = {0};
bool isFileSelected = false;
char errorMsg[256] = {0};
volatile sig_atomic_t should_exit = 0;

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        should_exit = 1;
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, signal_handler);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, TextFormat("Bytestream - v%s", BYTESTREAM_VERSION));
    SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
    SetWindowPosition(0, 0);
    // SetExitKey(KEY_LEFT_CONTROL & KEY_Q);
    SetTargetFPS(60);
    SetTextLineSpacing(0);

    Texture2D logo_texture = bundle_load_texture("./assets/logos/bytestream-bg.png");
    Image logo = LoadImageFromTexture(logo_texture);
    SetWindowIcon(logo);

    init_application(&app);

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

    while (!WindowShouldClose() && !should_exit)
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

            char *select_text = "Bytestream - v" BYTESTREAM_VERSION;
            float title_size = 48.0f;
            Font title_font = get_best_font(app.fonts, title_size, UI_FONT);
            Vector2 textSize1 = MeasureTextEx(title_font, select_text, title_size, 1.0f);
            Vector2 text1 = {(screenWidth - textSize1.x) / 2, logoY + logo_texture.height};

            Rectangle buttonRect = {
                (screenWidth - 200) / 2,
                text1.y + textSize1.y + padding,
                200,
                40,
            };

            Color buttonColor = SECONDARY_BGCOLOR;
            if (IsFileDropped())
            {
                FilePathList droppedFiles = LoadDroppedFiles();
                for (size_t i = 0; i < droppedFiles.count; i++)
                {
                    if (GetFileExtension(droppedFiles.paths[i]))
                    {
                        if (player_init(droppedFiles.paths[i]) < 0)
                        {
                            snprintf(errorMsg, sizeof(errorMsg), "Failed to initialize player with file: %s", droppedFiles.paths[i]);
                            break;
                        }
                        else
                        {
                            isFileSelected = true;
                        }
                    }
                }
                UnloadDroppedFiles(droppedFiles);
            }
            if (CheckCollisionPointRec(GetMousePosition(), buttonRect))
            {
                buttonColor = Fade(SECONDARY_BGCOLOR, 0.8f);
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
                        }
                    }
                }
            }
            else
            {
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            }

            BeginDrawing();
            ClearBackground(BACKGROUND_COLOR);
            DrawTexture(logo_texture, logoX, logoY, WHITE);
            DrawTextEx(title_font, select_text, text1, title_size, 0.0f, WHITE);

            float button_size = 24.0f;
            Font button_font = get_best_font(app.fonts, button_size, UI_FONT);
            const char *buttonText = "Select Video";
            Vector2 buttonTextSize = MeasureTextEx(button_font, buttonText, button_size, 0.0f);

            DrawRectangleRec(buttonRect, buttonColor);
            DrawTextEx(button_font, buttonText,
                       (Vector2){
                           buttonRect.x + (buttonRect.width - buttonTextSize.x) / 2,
                           buttonRect.y + (buttonRect.height - buttonTextSize.y) / 2},
                       button_size, 0.0f, WHITE);

            if (*errorMsg)
            {
                Font error_font = get_best_font(app.fonts, 20, UI_FONT);
                float error_size = 20.0f;
                Vector2 errorSize = MeasureTextEx(error_font, errorMsg, error_size, 1.0f);
                DrawTextEx(error_font, errorMsg,
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
    cleanup_application(&app);
    CloseWindow();
    return 0;
}
