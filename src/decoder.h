#ifndef DECODER_H
#define DECODER_H

#include <stdbool.h>
#include <pthread.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include "raylib.h"
#include "subtitle.h"

#include "queue.h"

struct DecoderState
{
    AVFormatContext *format_ctx;
    AVCodecContext *video_codec_ctx;
    AVCodecContext *audio_codec_ctx;
    AVCodecContext *subtitle_codec_ctx;
    int audio_stream_idx;
    int video_stream_idx;
    int subtitle_stream_idx;
    int selected_subtitle_stream_idx;
    AVStream *video_stream;
    AVStream *audio_stream;
    AVStream *subtitle_stream;
    AVPacket *packet;
    SwrContext *swr_ctx;
    struct SwsContext *sws_ctx;
    AVAudioFifo *fifo;
    AVDictionaryEntry *tag;

    uint8_t *rgba_frame_buffer;
    char *current_subtitle;

    struct Queue *audio_queue;
    struct Queue *video_queue;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t texture_mutex;
    pthread_t decode_thread;
    pthread_t video_thread;
    bool decoding;
    pthread_mutex_t pause_mutex;
};

typedef struct DecoderState DecoderState;
extern DecoderState ds;
extern int64_t frame_time;

int decoder_init(char *filename);
int decoder_decode_frame(void);
int decoder_change_audio(char *language);
int decoder_change_subtitle(char *language);
void decoder_stop(void);
void *decode_thread_func(void *arg);
void *video_thread_func(void *arg);

void pause_decoder(void);
void resume_decoder(void);

#endif // DECODER_H
