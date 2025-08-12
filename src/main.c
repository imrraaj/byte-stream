#include "decoder.h"
#include "player.h"
#include "tinyfiledialogs.h"
#include "raylib.h"
#include <stdio.h>

#define BYTESTREAM_VERSION "0.1.0"
bool isFileSelected = false;
char errorMsg[256] = {0};

int main(int argc, char **argv)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(800, 600, "Byte Stream Player");
    SetTargetFPS(60);

    Texture2D logo_texture = bundle_load_texture("./assets/logos/bytestream-256.png");
    Image logo = LoadImageFromTexture(logo_texture);
    SetWindowIcon(logo);

    Font google48 = bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 48);
    Font google26 = bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", 26);

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
            Vector2 textSize1 = MeasureTextEx(google48, select_text, 48, 0);
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
            DrawTextEx(google48, select_text, text1, 48, 0, WHITE);
            DrawRectangleRec(buttonRect, buttonColor);
            Vector2 buttonTextSize = MeasureTextEx(google26, "Select Video", 26, 0);
            DrawTextEx(google26, "Select Video",
                       (Vector2){
                           buttonRect.x + (buttonRect.width - buttonTextSize.x) / 2,
                           buttonRect.y + (buttonRect.height - buttonTextSize.y) / 2},
                       26, 0, WHITE);

            if (errorMsg[0] != '\0')
            {
                Vector2 errorSize = MeasureTextEx(google26, errorMsg, 26, 0);
                DrawTextEx(google26, errorMsg,
                           (Vector2){
                               (screenWidth - errorSize.x) / 2,
                               buttonRect.y + buttonRect.height + padding},
                           26, 0, RED);
            }
            EndDrawing();
        }
    }

    UnloadImage(logo);
    if (isFileSelected)
        player_close();
    return 0;
}
