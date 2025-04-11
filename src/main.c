#include "decoder.h"
#include "player.h"
#include "tinyfiledialogs.h"
#include "raylib.h"

#include <stdio.h>

int main(int argc, char **argv)
{
  char *file_name;
  if (argc < 2)
  {
    char const *lFilterPatterns[] = {"*.mp4"};
    file_name = tinyfd_openFileDialog("Please select a video file to play", ".", 1, lFilterPatterns, NULL, 1);
    if (!file_name)
    {
      tinyfd_messageBox("Error", "Invalid file selected", "ok", "error", 0);
      return 1;
    }
  }
  else
  {
    file_name = argv[1];
  }

  if (player_init(file_name) < 0)
  {
    fprintf(stderr, "ERROR: Could not initialize player\n");
    return -1;
  }

  while (!WindowShouldClose())
  {
    player_update();
  }

  player_close();
  return 0;
}
