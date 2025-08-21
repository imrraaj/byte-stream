#ifndef DECODER_H
#define DECODER_H

#include <pthread.h>
#include <stdbool.h>

#include "raylib.h"
#include "subtitle.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "queue.h"

typedef struct SubtitleItem {
    char *text;
    double start_time;
    double end_time;
    struct SubtitleItem *next;
} SubtitleItem;

struct DecoderState {
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
  double subtitle_start_time;
  double subtitle_end_time;
  SubtitleItem *subtitle_queue;
  pthread_mutex_t subtitle_mutex;

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
static bool sync_initialized;
static double playback_start_time;
static double first_frame_pts;
static double audio_clock;

int decoder_init(char *filename);
int decoder_decode_frame(void);
int decoder_change_audio(char *language);
int decoder_change_subtitle(char *language);
void decoder_stop(void);
void *decode_thread_func(void *arg);
void *video_thread_func(void *arg);
char *decoder_get_metadata(DecoderState *ds, char *key);
void pause_decoder(void);
void resume_decoder(void);
void reset_sync_state(void);
void add_subtitle(const char *text, double start_time, double end_time);
const char *get_current_subtitle(double current_time);
void clear_subtitle_queue(void);

#endif // DECODER_H
