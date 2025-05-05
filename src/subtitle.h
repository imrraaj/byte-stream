#ifndef SUBTITLE_H
#define SUBTITLE_H

#include <libavcodec/avcodec.h>

char *clean_subtitle(char *input, enum AVSubtitleType type);

#endif // SUBTITLE_H
