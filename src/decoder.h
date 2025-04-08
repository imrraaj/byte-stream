#ifndef DECODER_H
#define DECODER_H

#include <stdbool.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>

#include "raylib.h"

struct DecoderState
{
    AVFormatContext *format_ctx;
    AVCodecContext *video_codec_ctx;
    AVCodecContext *audio_codec_ctx;
    int audio_stream_idx;
    int video_stream_idx;
    AVStream *video_stream;
    AVStream *audio_stream;
    AVFrame *frame;
    AVPacket *packet;
    SwrContext *swr_ctx;
    struct SwsContext *sws_ctx;
    AVAudioFifo *fifo;
    AVDictionaryEntry *tag;

    uint8_t *rgba_frame_buffer;
};

typedef struct DecoderState DecoderState;
extern DecoderState ds;
extern int64_t frame_time;

int decoder_init(char *filename);
int decoder_decode_frame(Texture texture, int64_t *frame_time);
int decoder_fill_audio_queue(Texture texture, int64_t *frame_time);

#endif // DECODER_H
