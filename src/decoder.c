#include "decoder.h"
#include "raylib.h"
#include "subtitle.h"
#include "queue.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libavutil/dict.h>

#define FIFO_MIN_FRAMES 256 * 1024

DecoderState ds = {0};
int64_t frame_time = 0;
float playback_speed = 1.25;

static int indices[100];
static int num_audio_streams = 0;
static int indices_index = 0;

static int sub_indices[100];
static int sub_num_audio_streams = 0;
static int sub_indices_index = 0;

static double playback_start_time = 0.0;
static double first_frame_pts = 0.0;
static double pause_time = 0.0;

void check_pause(void)
{
    pthread_mutex_lock(&ds.pause_mutex);
    pthread_mutex_unlock(&ds.pause_mutex);
}
void pause_decoder(void)
{
    pause_time = GetTime();
    pthread_mutex_lock(&ds.pause_mutex);
}
void resume_decoder(void)
{
    playback_start_time += pause_time;
    pthread_mutex_unlock(&ds.pause_mutex);
}

int decoder_decode_frame(void)
{
    int read_bytes = av_read_frame(ds.format_ctx, ds.packet);
    if (read_bytes < 0)
        return -1;

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
    else if (ds.packet->stream_index == ds.subtitle_stream_idx)
    {
        AVSubtitle sub;
        int got_frame = 0;
        if (avcodec_decode_subtitle2(ds.subtitle_codec_ctx, &sub, &got_frame, ds.packet) >= 0 && got_frame)
        {
            for (size_t i = 0; i < sub.num_rects; i++)
            {
                WaitTime(1.0f / av_q2d(ds.subtitle_stream->r_frame_rate));
                if (sub.rects[i]->type == SUBTITLE_TEXT)
                {
                    printf("Subtitle (Text): %s\n", sub.rects[i]->text);
                    ds.current_subtitle = sub.rects[i]->text;
                }
                else if (sub.rects[i]->type == SUBTITLE_ASS)
                {
                    ds.current_subtitle = clean_subtitle(sub.rects[i]->ass, SUBTITLE_ASS);
                    printf("Subtitle (ASS): %s\n", sub.rects[i]->ass);
                }
                printf("Start Time: %u ms, End Time: %u ms\n", sub.start_display_time, sub.end_display_time);
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
            av_frame_get_buffer(video_frame, 0);
            while (avcodec_receive_frame(ds.video_codec_ctx, video_frame) == 0)
            {
                double pts_seconds = video_frame->pts * av_q2d(ds.video_stream->time_base);
                if (playback_start_time == 0.0)
                {
                    playback_start_time = GetTime();
                    first_frame_pts = pts_seconds;
                }
                double current_time = GetTime() - playback_start_time;
                double delay = (pts_seconds - first_frame_pts) - current_time;

                if (delay > 0)
                    WaitTime(delay);
                // WaitTime(1.0f / av_q2d(ds.video_stream->r_frame_rate));
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
        if (ds.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            indices[num_audio_streams] = i;
            num_audio_streams++;
        }
        else if (ds.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            sub_indices[sub_num_audio_streams++] = i;
        }
    }
    ds.video_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ds.audio_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    for (int i = 0; i < num_audio_streams; i++)
    {
        if (indices[i] == ds.audio_stream_idx)
        {
            indices_index = i;
            break;
        }
    }

    printf("Video stream index: %d\n", ds.video_stream_idx);
    printf("Audio stream index: %d\n", ds.audio_stream_idx);
    printf("Indices index: %d\n", indices_index);
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
    const AVCodec *video_codec = avcodec_find_decoder(ds.video_stream->codecpar->codec_id);
    const AVCodec *audio_codec = avcodec_find_decoder(ds.audio_stream->codecpar->codec_id);
    if (!video_codec)
    {
        fprintf(stderr, "ERROR: Could not find video codec\n");
        return -1;
    }
    if (!audio_codec)
    {
        fprintf(stderr, "ERROR: Could not find audio codec\n");
        return -1;
    }

    ds.video_codec_ctx = avcodec_alloc_context3(video_codec);
    ds.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (!ds.video_codec_ctx)
    {
        fprintf(stderr, "ERROR: Could not allocate video codec context\n");
        return -1;
    }
    if (!ds.audio_codec_ctx)
    {
        fprintf(stderr, "ERROR: Could not allocate audio codec context\n");
        return -1;
    }

    if (avcodec_parameters_to_context(ds.video_codec_ctx, ds.video_stream->codecpar) < 0)
    {
        fprintf(stderr, "ERROR: Could not set video codec parameters\n");
        return -1;
    }
    if (avcodec_parameters_to_context(ds.audio_codec_ctx, ds.audio_stream->codecpar) < 0)
    {
        fprintf(stderr, "ERROR: Could not set audio codec parameters\n");
        return -1;
    }

    if (avcodec_open2(ds.video_codec_ctx, video_codec, NULL) < 0)
    {
        fprintf(stderr, "ERROR: Could not open video codec\n");
        return -1;
    }
    if (avcodec_open2(ds.audio_codec_ctx, audio_codec, NULL) < 0)
    {
        fprintf(stderr, "ERROR: Could not open audio codec\n");
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
    ds.decoding = false;
    pthread_mutex_unlock(&ds.pause_mutex);
    pthread_join(ds.decode_thread, NULL);
    pthread_join(ds.video_thread, NULL);
    pthread_mutex_destroy(&ds.queue_mutex);
    pthread_mutex_destroy(&ds.texture_mutex);

    queue_free(ds.video_queue);
    queue_free(ds.audio_queue);

    av_packet_free(&ds.packet);
    avcodec_free_context(&ds.video_codec_ctx);
    avcodec_free_context(&ds.audio_codec_ctx);
    avcodec_free_context(&ds.subtitle_codec_ctx);
    sws_freeContext(ds.sws_ctx);
    swr_free(&ds.swr_ctx);
    av_audio_fifo_free(ds.fifo);
    free(ds.rgba_frame_buffer);
    avformat_close_input(&ds.format_ctx);
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

    indices_index = (indices_index + 1) % num_audio_streams;
    ds.audio_stream_idx = indices[indices_index];
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

    // ==========================================================
    // ============= NEW & CRITICAL SECTION START ===============
    // ==========================================================

    int64_t seek_target = (int64_t)(frame_time / av_q2d(ds.video_stream->time_base));
    if (avformat_seek_file(ds.format_ctx, ds.video_stream_idx, INT64_MIN, seek_target, INT64_MAX, 0) < 0)
    {
        fprintf(stderr, "ERROR: Seek failed during audio change!\n");
    }
    if (avformat_seek_file(ds.format_ctx, ds.audio_stream_idx, INT64_MIN, seek_target, INT64_MAX, 0) < 0)
    {
        fprintf(stderr, "ERROR: Seek failed during audio change!\n");
    }

    avcodec_flush_buffers(ds.video_codec_ctx);
    avcodec_flush_buffers(ds.audio_codec_ctx);
    queue_clear(ds.video_queue);
    av_audio_fifo_reset(ds.fifo);

    // ==========================================================
    // ============= NEW & CRITICAL SECTION END =================
    // ==========================================================

    // --- 7. Get language metadata for UI ---
    AVDictionaryEntry *tag = NULL;
    char lang[256] = "unknown";
    char title[256] = "";
    while ((tag = av_dict_get(ds.audio_stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        if (strcmp(tag->key, "language") == 0)
        {
            strncpy(lang, tag->value, sizeof(lang) - 1);
        }
        if (strcmp(tag->key, "title") == 0)
        {
            strncpy(title, tag->value, sizeof(title) - 1);
        }
    }
    if (title[0] != '\0')
    {
        snprintf(language, 512, "%s - %s", title, lang);
    }
    else
    {
        snprintf(language, 512, "%s", lang);
    }
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
    sub_indices_index = (sub_indices_index + 1) % sub_num_audio_streams;
    ds.subtitle_stream_idx = sub_indices[sub_indices_index];
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

    AVDictionaryEntry *tag = NULL;
    char lang[256] = "unknown";
    char title[256] = "";
    while ((tag = av_dict_get(ds.subtitle_stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        if (strcmp(tag->key, "language") == 0)
        {
            strncpy(lang, tag->value, sizeof(lang) - 1);
        }
        if (strcmp(tag->key, "title") == 0)
        {
            strncpy(title, tag->value, sizeof(title) - 1);
        }
    }
    if (title[0] != '\0')
    {
        snprintf(language, 512, "%s - %s", title, lang);
    }
    else
    {
        snprintf(language, 512, "%s", lang);
    }
    printf("Switched to language: %s\n", language);
    pthread_mutex_unlock(&ds.queue_mutex);
    pthread_mutex_unlock(&ds.pause_mutex);
    printf("Subtitle stream changed successfully.\n");
    return 0;
}
