#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "raylib.h"
#include "player.h"
#include "decoder.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Video file not provided\n");
        fprintf(stderr, "USAGE: %s <file>\n", argv[0]);
        exit(1);
    }

    char *file_name = argv[1];
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
