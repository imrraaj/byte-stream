#ifndef SUBTITLE_H
#define SUBTITLE_H

#include <libavcodec/avcodec.h>

char *clean_subtitle(char *input, enum AVSubtitleType type);
char *clean_vtt_subtitle(const char *input);
char *clean_srt_subtitle(const char *input);

#endif // SUBTITLE_H
