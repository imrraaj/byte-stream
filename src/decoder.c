#include "decoder.h"
#include "raylib.h"
#include "subtitle.h"
#include "queue.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libavutil/dict.h>

#define FIFO_MIN_FRAMES 256 * 1024

static void extract_stream_metadata(AVStream *stream, char *language, size_t lang_size)
{
    AVDictionaryEntry *tag = NULL;
    char lang[256] = "unknown", title[256] = "";
    
    while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (strcmp(tag->key, "language") == 0) {
            strncpy(lang, tag->value, sizeof(lang) - 1);
        } else if (strcmp(tag->key, "title") == 0) {
            strncpy(title, tag->value, sizeof(title) - 1);
        }
    }
    
    if (title[0] != '\0') {
        snprintf(language, lang_size, "%s - %s", title, lang);
    } else {
        snprintf(language, lang_size, "%s", lang);
    }
}

DecoderState ds = {0};
int64_t frame_time = 0;

static struct { int indices[100]; int count; int index; } audio_streams = {0};
static struct { int indices[100]; int count; int index; } subtitle_streams = {0};

static double playback_start_time = 0.0;
static double first_frame_pts = 0.0;
static double pause_time = 0.0;
static double audio_clock = 0.0;
static bool sync_initialized = false;

static inline void check_pause(void)
{
    pthread_mutex_lock(&ds.pause_mutex);
    pthread_mutex_unlock(&ds.pause_mutex);
}
void pause_decoder(void)
{
    pause_time = GetTime() - playback_start_time;
    pthread_mutex_lock(&ds.pause_mutex);
}
void resume_decoder(void)
{
    if (pause_time > 0)
    {
        playback_start_time = GetTime() - pause_time;
    }
    pause_time = 0.0;
    pthread_mutex_unlock(&ds.pause_mutex);
}

void reset_sync_state(void)
{
    sync_initialized = false;
    playback_start_time = 0.0;
    first_frame_pts = 0.0;
    pause_time = 0.0;
    audio_clock = 0.0;
    clear_subtitle_queue();
}

int decoder_decode_frame(void)
{
    int read_bytes = av_read_frame(ds.format_ctx, ds.packet);
    if (read_bytes < 0)
    {
        if (read_bytes == AVERROR_EOF)
        {
            // End of stream reached
            return -2;
        }
        return -1;
    }

    if (ds.packet->stream_index == ds.audio_stream_idx)
    {
        AVFrame *original_audio_frame = av_frame_alloc();
        if (avcodec_send_packet(ds.audio_codec_ctx, ds.packet) == 0)
        {
            AVFrame *resampled_frame = av_frame_alloc();
            resampled_frame->sample_rate = ds.audio_codec_ctx->sample_rate;
            AVChannelLayout out_ch_layout;
            av_channel_layout_default(&out_ch_layout, 2);
            av_channel_layout_copy(&resampled_frame->ch_layout, &out_ch_layout);
            resampled_frame->format = AV_SAMPLE_FMT_S16;
            av_frame_get_buffer(resampled_frame, 0);
            while (avcodec_receive_frame(ds.audio_codec_ctx, original_audio_frame) == 0)
            {
                swr_convert_frame(ds.swr_ctx, resampled_frame, original_audio_frame);

                pthread_mutex_lock(&ds.queue_mutex);
                double pts_seconds = original_audio_frame->pts * av_q2d(ds.audio_stream->time_base);
                audio_clock = pts_seconds + (double)resampled_frame->nb_samples / ds.audio_codec_ctx->sample_rate;
                pthread_mutex_unlock(&ds.queue_mutex);
            }
            av_audio_fifo_write(ds.fifo, (void **)resampled_frame->data, resampled_frame->nb_samples);
            av_frame_free(&resampled_frame);
        }
    }
    else if (ds.packet->stream_index == ds.video_stream_idx)
    {
        AVPacket *video_pkt = av_packet_clone(ds.packet);
        pthread_mutex_lock(&ds.queue_mutex);
        queue_push(ds.video_queue, video_pkt);
        pthread_mutex_unlock(&ds.queue_mutex);
    }
    else if (ds.packet->stream_index == ds.subtitle_stream_idx && ds.subtitle_codec_ctx)
    {
        AVSubtitle sub;
        int got_frame = 0;
        if (avcodec_decode_subtitle2(ds.subtitle_codec_ctx, &sub, &got_frame, ds.packet) >= 0 && got_frame)
        {
            int64_t subtitle_pts_video_timebase = av_rescale_q(ds.packet->pts, 
                                                             ds.subtitle_stream->time_base, 
                                                             ds.video_stream->time_base);
            double start_time = subtitle_pts_video_timebase * av_q2d(ds.video_stream->time_base);

            double duration = (sub.end_display_time > 0) ? 
                             (sub.end_display_time / 1000.0) : 
                             4.0; // 4 second default duration
                             
            double end_time = start_time + duration;
            
            for (size_t i = 0; i < sub.num_rects; i++)
            {
                if (sub.rects[i]->type == SUBTITLE_TEXT && sub.rects[i]->text)
                {
                    add_subtitle(sub.rects[i]->text, start_time, end_time);
                }
                else if (sub.rects[i]->type == SUBTITLE_ASS && sub.rects[i]->ass)
                {
                    char *cleaned = clean_subtitle(sub.rects[i]->ass, SUBTITLE_ASS);
                    if (cleaned)
                    {
                        add_subtitle(cleaned, start_time, end_time);
                        free(cleaned);
                    }
                }
            }
            avsubtitle_free(&sub);
        }
    }
    return 0;
}

void *decode_thread_func(void *arg)
{
    (void)arg;
    while (ds.decoding)
    {
        check_pause();
        decoder_decode_frame();
    }
    return NULL;
}
void *video_thread_func(void *arg)
{
    (void)arg;
    AVFrame *video_frame = av_frame_alloc();
    if (!video_frame)
    {
        fprintf(stderr, "ERROR: Could not allocate video frame\n");
        return NULL;
    }
    while (ds.decoding)
    {
        check_pause();
        pthread_mutex_lock(&ds.queue_mutex);
        AVPacket *pkt = queue_pop(ds.video_queue);
        pthread_mutex_unlock(&ds.queue_mutex);

        if (!pkt)
        {
            WaitTime(0.1);
            continue;
        }

        if (avcodec_send_packet(ds.video_codec_ctx, pkt) == 0)
        {
            // Frame buffer already allocated
            while (avcodec_receive_frame(ds.video_codec_ctx, video_frame) == 0)
            {
                double pts_seconds = video_frame->pts * av_q2d(ds.video_stream->time_base);

                if (!sync_initialized)
                {
                    playback_start_time = GetTime();
                    first_frame_pts = pts_seconds;
                    sync_initialized = true;
                }

                double current_time = GetTime() - playback_start_time;
                double video_time = pts_seconds - first_frame_pts;

                pthread_mutex_lock(&ds.queue_mutex);
                double audio_time = audio_clock - first_frame_pts;
                pthread_mutex_unlock(&ds.queue_mutex);

                // Calculate proper frame delay based on frame rate
                double frame_rate = av_q2d(ds.video_stream->r_frame_rate);
                double frame_delay = 1.0 / frame_rate;

                // Simple and stable audio-video synchronization
                double sync_threshold = 0.1; // 100ms threshold - more tolerant
                double av_diff = video_time - audio_time;

                // Calculate target delay based on video timing
                double target_delay = video_time - current_time;

                // Apply gentle correction based on audio-video difference
                if (av_diff > sync_threshold)
                {
                    // Video is ahead - add small correction
                    target_delay += av_diff * 0.1;
                }
                else if (av_diff < -sync_threshold)
                {
                    // Audio is ahead - reduce delay slightly
                    target_delay *= 0.8;
                }

                // Apply the delay with reasonable bounds
                if (target_delay > 0 && target_delay < 0.2)
                {
                    WaitTime(target_delay);
                }
                else if (target_delay <= 0)
                {
                    // Use minimal frame delay when behind
                    WaitTime(frame_delay * 0.2);
                }
                else
                {
                    // Cap excessive delays
                    WaitTime(0.2);
                }
                pthread_mutex_lock(&ds.texture_mutex);
                uint8_t *rgba_planes[] = {ds.rgba_frame_buffer};
                frame_time = (int64_t)(video_frame->pts * av_q2d(ds.video_stream->time_base));
                int rgba_linesizes[] = {ds.video_codec_ctx->width * 4};
                sws_scale(ds.sws_ctx, (const uint8_t *const *)video_frame->data, video_frame->linesize, 0, ds.video_codec_ctx->height, rgba_planes, rgba_linesizes);
                pthread_mutex_unlock(&ds.texture_mutex);
            }
        }
        av_packet_free(&pkt);
    }
    av_frame_free(&video_frame);
    return NULL;
}

int decoder_init(char *filename)
{
    if (avformat_open_input(&ds.format_ctx, filename, NULL, NULL) < 0)
    {
        fprintf(stderr, "ERROR: Could not open file: %s\n", filename);
        return -1;
    }

    if (avformat_find_stream_info(ds.format_ctx, NULL) < 0)
    {
        fprintf(stderr, "ERROR: Could not find stream info\n");
        return -1;
    }

    for (unsigned int i = 0; i < ds.format_ctx->nb_streams; i++)
    {
        enum AVMediaType type = ds.format_ctx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_AUDIO) {
            audio_streams.indices[audio_streams.count++] = i;
        } else if (type == AVMEDIA_TYPE_SUBTITLE) {
            subtitle_streams.indices[subtitle_streams.count++] = i;
        }
    }
    ds.video_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ds.audio_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    ds.subtitle_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_SUBTITLE, -1, -1, NULL, 0);

    for (int i = 0; i < audio_streams.count; i++) {
        if (audio_streams.indices[i] == ds.audio_stream_idx) {
            audio_streams.index = i;
            break;
        }
    }
    for (int i = 0; i < subtitle_streams.count; i++) {
        if (subtitle_streams.indices[i] == ds.subtitle_stream_idx) {
            subtitle_streams.index = i;
            break;
        }
    }

    printf("Streams - Video: %d, Audio: %d, Subtitle: %d\n", ds.video_stream_idx, ds.audio_stream_idx, ds.subtitle_stream_idx);
    if (ds.video_stream_idx < 0)
    {
        fprintf(stderr, "ERROR: Could not find video stream\n");
        return -1;
    }
    if (ds.audio_stream_idx < 0)
    {
        fprintf(stderr, "ERROR: Could not find audio stream\n");
        return -1;
    }

    ds.video_stream = ds.format_ctx->streams[ds.video_stream_idx];
    ds.audio_stream = ds.format_ctx->streams[ds.audio_stream_idx];
    ds.subtitle_stream = (ds.subtitle_stream_idx >= 0) ? ds.format_ctx->streams[ds.subtitle_stream_idx] : NULL;
    
    const AVCodec *video_codec = avcodec_find_decoder(ds.video_stream->codecpar->codec_id);
    const AVCodec *audio_codec = avcodec_find_decoder(ds.audio_stream->codecpar->codec_id);
    const AVCodec *subtitle_codec = ds.subtitle_stream ? avcodec_find_decoder(ds.subtitle_stream->codecpar->codec_id) : NULL;
    if (!video_codec || !audio_codec) {
        fprintf(stderr, "ERROR: Could not find required codecs\n");
        return -1;
    }

    ds.video_codec_ctx = avcodec_alloc_context3(video_codec);
    ds.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    ds.subtitle_codec_ctx = subtitle_codec ? avcodec_alloc_context3(subtitle_codec) : NULL;
    
    if (!ds.video_codec_ctx || !ds.audio_codec_ctx) {
        fprintf(stderr, "ERROR: Could not allocate codec contexts\n");
        return -1;
    }

    if (avcodec_parameters_to_context(ds.video_codec_ctx, ds.video_stream->codecpar) < 0 ||
        avcodec_parameters_to_context(ds.audio_codec_ctx, ds.audio_stream->codecpar) < 0 ||
        (ds.subtitle_codec_ctx && avcodec_parameters_to_context(ds.subtitle_codec_ctx, ds.subtitle_stream->codecpar) < 0)) {
        fprintf(stderr, "ERROR: Could not set codec parameters\n");
        return -1;
    }

    if (avcodec_open2(ds.video_codec_ctx, video_codec, NULL) < 0 ||
        avcodec_open2(ds.audio_codec_ctx, audio_codec, NULL) < 0 ||
        (ds.subtitle_codec_ctx && avcodec_open2(ds.subtitle_codec_ctx, subtitle_codec, NULL) < 0)) {
        fprintf(stderr, "ERROR: Could not open codecs\n");
        return -1;
    }

    // ds.frame = av_frame_alloc();
    ds.packet = av_packet_alloc();

    ds.sws_ctx = sws_getContext(ds.video_codec_ctx->width, ds.video_codec_ctx->height, ds.video_codec_ctx->pix_fmt, ds.video_codec_ctx->width, ds.video_codec_ctx->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

    if (!ds.sws_ctx)
    {
        fprintf(stderr, "ERROR: Could not create SwsContext\n");
        return -1;
    }

    ds.swr_ctx = swr_alloc();
    if (!ds.swr_ctx)
    {
        fprintf(stderr, "ERROR: Could not allocate SWRContext\n");
        return -1;
    }

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2);

    int ret = swr_alloc_set_opts2(&ds.swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16, ds.audio_codec_ctx->sample_rate, &ds.audio_codec_ctx->ch_layout, ds.audio_codec_ctx->sample_fmt, ds.audio_codec_ctx->sample_rate, 0, NULL);

    if (ret < 0)
    {
        fprintf(stderr, "ERROR: swr_alloc_set_opts2() failed\n");
        return -1;
    }

    if (swr_init(ds.swr_ctx) < 0)
    {
        fprintf(stderr, "ERROR: Could not initialize SwrContext\n");
        return -1;
    }
    ds.rgba_frame_buffer = malloc(ds.video_codec_ctx->width * ds.video_codec_ctx->height * 4);
    memset(ds.rgba_frame_buffer, 0, ds.video_codec_ctx->width * ds.video_codec_ctx->height * 4);
    ds.fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16, 2, FIFO_MIN_FRAMES * 2);
    ds.selected_subtitle_stream_idx = -1;
    ds.current_subtitle = malloc(1024);
    if (ds.current_subtitle) {
        ds.current_subtitle[0] = '\0';
    }
    ds.subtitle_start_time = -1.0;
    ds.subtitle_end_time = -1.0;
    ds.subtitle_queue = NULL;
    pthread_mutex_init(&ds.subtitle_mutex, NULL);

    ds.video_queue = queue_init();
    if (!ds.video_queue)
    {
        fprintf(stderr, "ERROR: Could not initialize video queue\n");
        return -1;
    }
    pthread_mutex_init(&ds.queue_mutex, NULL);
    pthread_mutex_init(&ds.texture_mutex, NULL);
    pthread_mutex_init(&ds.pause_mutex, NULL);
    ds.decoding = true;
    pthread_create(&ds.decode_thread, NULL, decode_thread_func, NULL);
    pthread_create(&ds.video_thread, NULL, video_thread_func, NULL);
    return 0;
}

void decoder_stop(void)
{
    // Signal threads to stop
    ds.decoding = false;

    // Ensure threads aren't stuck in pause - unlock if locked
    if (pthread_mutex_trylock(&ds.pause_mutex) != 0)
    {
        pthread_mutex_unlock(&ds.pause_mutex);
    }
    pthread_mutex_unlock(&ds.pause_mutex);

    // Wait for threads to finish gracefully
    pthread_join(ds.decode_thread, NULL);
    pthread_join(ds.video_thread, NULL);

    // Clean up mutexes
    pthread_mutex_destroy(&ds.queue_mutex);
    pthread_mutex_destroy(&ds.texture_mutex);
    pthread_mutex_destroy(&ds.pause_mutex);

    // Clean up video queue
    if (ds.video_queue) {
        queue_free(ds.video_queue);
        ds.video_queue = NULL;
    }

    // Clean up FFmpeg resources
    av_packet_free(&ds.packet);
    avcodec_free_context(&ds.video_codec_ctx);
    avcodec_free_context(&ds.audio_codec_ctx);
    avcodec_free_context(&ds.subtitle_codec_ctx);
    sws_freeContext(ds.sws_ctx);
    swr_free(&ds.swr_ctx);
    av_audio_fifo_free(ds.fifo);
    free(ds.rgba_frame_buffer);
    free(ds.current_subtitle);
    
    ds.packet = NULL;
    ds.video_codec_ctx = ds.audio_codec_ctx = ds.subtitle_codec_ctx = NULL;
    ds.sws_ctx = NULL; ds.swr_ctx = NULL; ds.fifo = NULL;
    ds.rgba_frame_buffer = NULL; ds.current_subtitle = NULL;
    clear_subtitle_queue();
    pthread_mutex_destroy(&ds.subtitle_mutex);
    avformat_close_input(&ds.format_ctx);
    ds.format_ctx = NULL;
}

int decoder_change_audio(char *language)
{
    pthread_mutex_lock(&ds.pause_mutex);
    pthread_mutex_lock(&ds.queue_mutex);
    printf("Changing audio stream...\n");
    if (ds.audio_codec_ctx)
    {
        avcodec_free_context(&ds.audio_codec_ctx);
    }
    if (ds.swr_ctx)
    {
        swr_free(&ds.swr_ctx);
    }

    audio_streams.index = (audio_streams.index + 1) % audio_streams.count;
    ds.audio_stream_idx = audio_streams.indices[audio_streams.index];
    ds.audio_stream = ds.format_ctx->streams[ds.audio_stream_idx];
    printf("Switching to audio stream index: %d\n", ds.audio_stream_idx);

    const AVCodec *audio_codec = avcodec_find_decoder(ds.audio_stream->codecpar->codec_id);
    ds.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(ds.audio_codec_ctx, ds.audio_stream->codecpar);
    if (avcodec_open2(ds.audio_codec_ctx, audio_codec, NULL) < 0)
    {
        fprintf(stderr, "ERROR: Could not open new audio codec\n");
        resume_decoder();
        return -1;
    }

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2);
    swr_alloc_set_opts2(&ds.swr_ctx,
                        &out_ch_layout, AV_SAMPLE_FMT_S16, ds.audio_codec_ctx->sample_rate,
                        &ds.audio_codec_ctx->ch_layout, ds.audio_codec_ctx->sample_fmt, ds.audio_codec_ctx->sample_rate,
                        0, NULL);
    if (swr_init(ds.swr_ctx) < 0)
    {
        fprintf(stderr, "ERROR: Failed to initialize the new SwrContext\n");
        resume_decoder();
        return -1;
    }

    // Resynchronize streams
    int64_t seek_target = (int64_t)(frame_time / av_q2d(ds.video_stream->time_base));
    if (avformat_seek_file(ds.format_ctx, ds.video_stream_idx, INT64_MIN, seek_target, INT64_MAX, 0) < 0 ||
        avformat_seek_file(ds.format_ctx, ds.audio_stream_idx, INT64_MIN, seek_target, INT64_MAX, 0) < 0) {
        fprintf(stderr, "ERROR: Seek failed during audio change!\n");
    }

    avcodec_flush_buffers(ds.video_codec_ctx);
    avcodec_flush_buffers(ds.audio_codec_ctx);
    queue_clear(ds.video_queue);
    av_audio_fifo_reset(ds.fifo);
    reset_sync_state();

    extract_stream_metadata(ds.audio_stream, language, 512);
    printf("Switched to language: %s\n", language);

    // --- 8. Resume processing ---
    pthread_mutex_unlock(&ds.queue_mutex);
    pthread_mutex_unlock(&ds.pause_mutex);
    printf("Audio stream changed successfully.\n");
    return 0;
}

int decoder_change_subtitle(char *language)
{
    pthread_mutex_lock(&ds.pause_mutex);
    pthread_mutex_lock(&ds.queue_mutex);
    if (ds.subtitle_codec_ctx)
    {
        avcodec_free_context(&ds.subtitle_codec_ctx);
    }
    clear_subtitle_queue();
    if (subtitle_streams.count > 0) {
        subtitle_streams.index = (subtitle_streams.index + 1) % subtitle_streams.count;
        ds.subtitle_stream_idx = subtitle_streams.indices[subtitle_streams.index];
    } else {
        printf("No subtitle streams available.\n");
        pthread_mutex_unlock(&ds.queue_mutex);
        pthread_mutex_unlock(&ds.pause_mutex);
        return -1;
    }
    ds.subtitle_stream = ds.format_ctx->streams[ds.subtitle_stream_idx];

    const AVCodec *sub_codec = avcodec_find_decoder(ds.subtitle_stream->codecpar->codec_id);
    ds.subtitle_codec_ctx = avcodec_alloc_context3(sub_codec);
    avcodec_parameters_to_context(ds.subtitle_codec_ctx, ds.subtitle_stream->codecpar);
    if (avcodec_open2(ds.subtitle_codec_ctx, sub_codec, NULL) < 0)
    {
        fprintf(stderr, "ERROR: Could not open new audio codec\n");
        resume_decoder();
        return -1;
    }

    extract_stream_metadata(ds.subtitle_stream, language, 512);
    printf("Switched to language: %s\n", language);
    pthread_mutex_unlock(&ds.queue_mutex);
    pthread_mutex_unlock(&ds.pause_mutex);
    printf("Subtitle stream changed successfully.\n");
    return 0;
}

char *decoder_get_metadata(DecoderState *ds, char *key)
{
    while ((ds->tag = av_dict_get(ds->format_ctx->metadata, "", ds->tag, AV_DICT_IGNORE_SUFFIX)))
    {
        if (strcmp(ds->tag->key, key) == 0)
        {
            return ds->tag->value;
        }
    }
    return "Untitled";
}

void add_subtitle(const char *text, double start_time, double end_time)
{
    SubtitleItem *new_item = malloc(sizeof(SubtitleItem));
    if (!new_item) return;
    
    new_item->text = strdup(text);
    new_item->start_time = start_time;
    new_item->end_time = end_time;
    new_item->next = NULL;
    
    pthread_mutex_lock(&ds.subtitle_mutex);
    
    if (!ds.subtitle_queue) {
        ds.subtitle_queue = new_item;
    } else {
        // Insert in chronological order
        SubtitleItem *current = ds.subtitle_queue;
        SubtitleItem *prev = NULL;
        
        while (current && current->start_time < start_time) {
            prev = current;
            current = current->next;
        }
        
        if (prev) {
            prev->next = new_item;
            new_item->next = current;
        } else {
            new_item->next = ds.subtitle_queue;
            ds.subtitle_queue = new_item;
        }
    }
    
    pthread_mutex_unlock(&ds.subtitle_mutex);
}

const char *get_current_subtitle(double current_time)
{
    pthread_mutex_lock(&ds.subtitle_mutex);
    
    for (SubtitleItem *current = ds.subtitle_queue; current; current = current->next) {
        if (current_time >= current->start_time && current_time <= current->end_time) {
            pthread_mutex_unlock(&ds.subtitle_mutex);
            return current->text;
        }
    }
    
    pthread_mutex_unlock(&ds.subtitle_mutex);
    return NULL;
}

void clear_subtitle_queue(void)
{
    pthread_mutex_lock(&ds.subtitle_mutex);
    
    while (ds.subtitle_queue) {
        SubtitleItem *next = ds.subtitle_queue->next;
        free(ds.subtitle_queue->text);
        free(ds.subtitle_queue);
        ds.subtitle_queue = next;
    }
    
    pthread_mutex_unlock(&ds.subtitle_mutex);
}
