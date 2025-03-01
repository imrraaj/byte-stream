#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "raylib.h"

int main(int argc, char **argv) {

    if(argc < 2) {
        fprintf(stderr, "Video file not provided\n");
        fprintf(stderr, "USAGE: %s <file>\n", argv[0]);
        exit(1);
    }

    char *file_name = argv[1];
    char info_cmd[256];
    snprintf(
        info_cmd, 
        sizeof(info_cmd), 
        "ffprobe -v error -select_streams v:0 -show_entries stream=width,height:format_tags=title -of csv=s=x:p=0 %s",
        file_name
    );

    FILE *info_pipe = popen(info_cmd, "r");
    if (!info_pipe) {
        perror("Error opening ffmpeg for information. Did you install ffmpeg?");
        return 1;
    }

    int frame_width = 0, frame_height = 0;
    char line[256];
    fgets(line, sizeof(line), info_pipe);
    sscanf(line, "%dx%d", &frame_width, &frame_height);


    char metadata[1024];
    fgets(metadata, sizeof(metadata), info_pipe);
    pclose(info_pipe);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffmpeg -i %s -f rawvideo -pix_fmt rgba -s %dx%d pipe:1", file_name, frame_width, frame_height);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
      perror("Error opening ffmpeg for frames. Did you install ffmpeg?");
      return 1;
    }

  
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(800, 600, file_name);
    SetTargetFPS(60);
  
    size_t frame_size = frame_width * frame_height * 4;
    uint8_t *frame = malloc(frame_size);
  
    Image image = {
      .data = frame,
      .width = frame_width,
      .height = frame_height,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
      .mipmaps = 1
    };
  
    Texture2D texture = LoadTextureFromImage(image);
  
  
    int paused = 0;
    while (!WindowShouldClose()) {
  
      if(IsKeyPressed(KEY_SPACE)){
        paused = 1 - paused;
        printf("clicked pause %d\n", paused);
      }
      if (paused == 1) continue;
  
      UpdateTexture(texture, frame);
      size_t read_bytes = fread(frame, 1, frame_size, pipe);
      if (read_bytes < frame_size) {
        if (feof(pipe)) {
          break;
        } else {
          perror("Error while reading a frame");
          break;
        }
      }
  
      // I want to scale the image to fit the window
      printf("%d, %d\n", GetScreenWidth(), GetScreenHeight());
      float scaleX = GetScreenWidth() / (float)frame_width;
      float scaleY = GetScreenHeight() / (float)frame_height;
      float scale = scaleX < scaleY ? scaleX : scaleY; 
      printf("scale: %f\n", scale);
  
      // Calculate the final dimensions
      int dest_width = frame_width * scale;
      int dest_height = frame_height * scale;
  
      BeginDrawing();
      ClearBackground(RED);
      DrawTexturePro(texture,
                     (Rectangle){0, 0, frame_width, frame_height},
                     (Rectangle){0, 0, dest_width, frame_height},
                     (Vector2){0, 0}, 0.0f, WHITE);
      EndDrawing();
    }
    UnloadTexture(texture);
    free(frame);
    pclose(pipe);
    return 0;
}
