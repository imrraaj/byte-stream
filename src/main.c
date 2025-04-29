#include "decoder.h"
#include "player.h"
#include "tinyfiledialogs.h"
#include "raylib.h"

#include <stdio.h>

bool isFileSelected = false;
int main(int argc, char** argv)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(800, 600, "Byte Stream Player");
    SetTargetFPS(60);
    SetWindowIcon(LoadImage("assets/logo.png"));

    Font google = LoadFontEx("assets/CircularSpotifyText-Bold.otf", 20, 0, 0);
    if (argc > 1){
        isFileSelected = true;
        if (player_init(argv[1]) < 0)
        {
            fprintf(stderr, "ERROR: Failed to initialize player with file: %s\n", argv[1]);
            exit(1);
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
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                char *file_name = tinyfd_openFileDialog("Please select a video file to play", ".", 1, NULL, NULL, 1);
                if (file_name)
                {
                    isFileSelected = true;
                    if (player_init(file_name) < 0)
                    {
                        fprintf(stderr, "ERROR: Failed to initialize player with file: %s\n", file_name);
                        exit(1);
                    }
                }
            }
            BeginDrawing();
            ClearBackground(GetColor(0x181818FF));
            DrawTextEx(google, "Click to select a video file", (Vector2){10, 10}, 20, 0, WHITE);
            DrawTextEx(google, "Press ESC to exit", (Vector2){10, 60}, 20, 0, WHITE);
            EndDrawing();
        }
    }

    player_close();
    return 0;
}
