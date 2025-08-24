#include "subtitle.h"
#include <libavcodec/avcodec.h>
#include <stdlib.h>
#include <string.h>

char *clean_ass_subtitle(const char *line) {
    if (!line)
        return NULL;
    size_t len = strlen(line);
    char *output = malloc(len + 1);
    if (!output)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        if (line[i] == '{') {
            while (i < len && line[i] != '}' && line[i] != '\0')
                i++;
            if (i < len && line[i] == '}')
                i++;
            if (i >= len)
                break;
            i--;
        } else if (line[i] == '\\' && i + 1 < len) {
            char next = line[i + 1];
            switch (next) {
            case 'N':
            case 'n':
                output[j++] = '\n';
                i++;
                break;
            case 'h':
                output[j++] = ' ';
                i++;
                break;
            default:
                output[j++] = line[i];
                break;
            }
        } else
            output[j++] = line[i];
    }
    output[j] = '\0';
    char *result = realloc(output, j + 1);
    return result ? result : output;
}
char *get_ass_subtitle_text(const char *csv_line) {
    char *line = strdup(csv_line);
    char *start = line;
    int field = 0;
    while (field < 8 && *start) {
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

char *clean_vtt_subtitle(const char *input) {
    if (!input)
        return NULL;
    size_t len = strlen(input);
    char *output = malloc(len + 1);
    if (!output)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] == '\\' && i + 1 < len) {
            char next = input[i + 1];
            switch (next) {
            case 'h':
                output[j++] = ' ';
                i++;
                break;
            case 'n':
            case 'N':
                output[j++] = '\n';
                i++;
                break;
            default:
                output[j++] = input[i];
                break;
            }
        } else if (input[i] == '<') {
            while (i < len && input[i] != '>' && input[i] != '\0')
                i++;
            if (i < len && input[i] == '>')
                i++;
            if (i < len)
                i--;
        } else if (input[i] == '&') {
            if (strncmp(&input[i], "&amp;", 5) == 0) {
                output[j++] = '&';
                i += 4;
            } else if (strncmp(&input[i], "&lt;", 4) == 0) {
                output[j++] = '<';
                i += 3;
            } else if (strncmp(&input[i], "&gt;", 4) == 0) {
                output[j++] = '>';
                i += 3;
            } else if (strncmp(&input[i], "&nbsp;", 6) == 0) {
                output[j++] = ' ';
                i += 5;
            } else {
                output[j++] = input[i];
            }
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
    char *result = realloc(output, j + 1);
    return result ? result : output;
}

char *clean_srt_subtitle(const char *input) {
    if (!input)
        return NULL;
    size_t len = strlen(input);
    char *output = malloc(len + 1);
    if (!output)
        return NULL;
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] == '<') {
            while (i < len && input[i] != '>' && input[i] != '\0')
                i++;
            if (i < len && input[i] == '>')
                i++;
            if (i < len)
                i--;
        } else if (input[i] == '\\' && i + 1 < len) {
            char next = input[i + 1];
            if (next == 'n' || next == 'N') {
                output[j++] = '\n';
                i++;
            } else {
                output[j++] = input[i];
            }
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
    char *result = realloc(output, j + 1);
    return result ? result : output;
}

char *clean_subtitle(char *input, enum AVSubtitleType type) {
    if (!input)
        return NULL;
    switch (type) {
    case SUBTITLE_ASS:
        return clean_ass_subtitle(get_ass_subtitle_text(input));
    case SUBTITLE_TEXT:
        if (strstr(input, "\\h"))
            return clean_vtt_subtitle(input);
        else if (strstr(input, "<") && strstr(input, ">"))
            return clean_srt_subtitle(input);
        else
            return strdup(input);
    default:
        return strdup(input);
    }
}
