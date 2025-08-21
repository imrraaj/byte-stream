#include "subtitle.h"
#include <libavcodec/avcodec.h>
#include <string.h>
#include <stdlib.h>

char *clean_ass_subtitle(const char *line)
{
    size_t len = strlen(line);
    char *output = malloc(len + 1); // Allocate enough space for the input string
    if (!output)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; line[i] != '\0'; i++)
    {
        if (line[i] == '{')
        {
            // Skip until closing '}' or end of string
            while (line[i] != '}' && line[i] != '\0')
                i++;
            if (line[i] == '}')
                i++; // Skip the closing '}'
            if (line[i] == '\0')
                break; // Exit if end of string
        }
        if (line[i] == '\\' && line[i + 1] == 'N')
        {
            // Convert \N to newline
            output[j++] = '\n';
            i++; // Skip 'N'
        }
        else
        {
            output[j++] = line[i];
        }
    }
    output[j] = '\0';

    // Reallocate to fit the exact size
    char *result = realloc(output, j + 1);
    return result ? result : output;
}
char *get_ass_subtitle_text(const char *csv_line)
{
    char *line = strdup(csv_line);
    char *start = line;
    int field = 0;
    while (field < 8 && *start)
    {
        if (*start == ',')
            field++;
        start++;
    }
    if (*start == ',')
        start++;
    char *result = strdup(start);
    free(line);
    return result;
}

char *clean_subtitle(char *input, enum AVSubtitleType type)
{
    if (type == SUBTITLE_ASS)
    {
        return clean_ass_subtitle(get_ass_subtitle_text(input));
    }
    return "";
}
