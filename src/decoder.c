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

#define FIFO_MIN_FRAMES 1

DecoderState ds = {0};
int64_t frame_time = 0;
float playback_speed = 1.25;
int audio_stream_indices[10];
int num_audio_streams = 0;
int current_audio_index = 0;
int subtitle_stream_indices[10];
int num_subtitle_streams = 0;


int decoder_decode_frame(Texture texture, int64_t *frame_time)
{
    int read_bytes = av_read_frame(ds.format_ctx, ds.packet);
    if (read_bytes < 0)
        return -1;

    if (ds.packet->stream_index == ds.audio_stream_idx)
    {
        if (avcodec_send_packet(ds.audio_codec_ctx, ds.packet) == 0)
        {
            AVFrame *resampled_frame = av_frame_alloc();
            resampled_frame->sample_rate = ds.audio_codec_ctx->sample_rate;
            AVChannelLayout out_ch_layout;
            av_channel_layout_default(&out_ch_layout, 2);
            av_channel_layout_copy(&resampled_frame->ch_layout, &out_ch_layout);
            resampled_frame->format = AV_SAMPLE_FMT_S16;
            av_frame_get_buffer(resampled_frame, 0);
            while (avcodec_receive_frame(ds.audio_codec_ctx, ds.frame) == 0)
            {
                swr_convert_frame(ds.swr_ctx, resampled_frame, ds.frame);
            }
            av_audio_fifo_write(ds.fifo, (void **)resampled_frame->data, resampled_frame->nb_samples);
            av_frame_free(&resampled_frame);
        }
    }

   return 0;
}

void *decode_thread_func(void *arg)
{
    DecoderState *ds = (DecoderState *)arg;
    // Print all audio information
    // printf(
    
    while (ds->decoding)
    {
        decoder_decode_frame(ps.texture, &frame_time);
        // decoder_fill_audio_queue(ps.texture, &frame_time);
    }
    // DecoderState *ds = (DecoderState *)arg; // Suppress unused parameter warning
    // while (ds->decoding)
    // {

    //     int read_bytes = av_read_frame(ds->format_ctx, ds->packet);
    //     if (read_bytes < 0)
    //         return NULL;
    //     if (ds->packet->stream_index == ds->video_stream_idx)
    //     {
    //         pthread_mutex_lock(&ds->queue_mutex);
    //         queue_push(ds->video_queue, ds->packet);
    //         pthread_mutex_unlock(&ds->queue_mutex);
    //     }
    //     else
    //     {
    //         av_packet_free(&ds->packet);
    //     }
    // }
    // return NULL;
    // DecoderState *ds = (DecoderState *)arg;
    // while (ds->decoding)
    // {
    //
    // }
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
            audio_stream_indices[num_audio_streams++] = i;
        }
        else if (ds.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            subtitle_stream_indices[num_subtitle_streams++] = i;
        }
    }

    ds.video_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ds.audio_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
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

    ds.frame = av_frame_alloc();
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
    ds.audio_queue = queue_init();
    if (!ds.audio_queue)
    {
        fprintf(stderr, "ERROR: Could not initialize audio queue\n");
        queue_free(ds.audio_queue);
        return -1;
    }

    pthread_mutex_init(&ds.queue_mutex, NULL);
    ds.decoding = true;
    pthread_create(&ds.decode_thread, NULL, decode_thread_func, &ds);

    return 0;
}
int decoder_fill_audio_queue(Texture texture, int64_t *frame_time)
{
    pthread_mutex_lock(&ds.queue_mutex);
    while (av_audio_fifo_size(ds.fifo) <= FIFO_MIN_FRAMES)
    {
        if (av_read_frame(ds.format_ctx, ds.packet) == 0 && ds.packet->stream_index == ds.audio_stream_idx)
        {
            if (avcodec_send_packet(ds.audio_codec_ctx, ds.packet) != 0)
            {
                fprintf(stderr, "ERROR: Error sending packet\n");
                return -1;
            }
            while (avcodec_receive_frame(ds.audio_codec_ctx, ds.frame) == 0)
            {
                AVFrame *resampled_frame = av_frame_alloc();
                resampled_frame->sample_rate = ds.audio_codec_ctx->sample_rate;
                AVChannelLayout out_ch_layout;
                av_channel_layout_default(&out_ch_layout, 2);
                av_channel_layout_copy(&resampled_frame->ch_layout, &out_ch_layout);
                
                resampled_frame->format = AV_SAMPLE_FMT_S16;
                resampled_frame->nb_samples = ds.frame->nb_samples;
                
                swr_convert_frame(ds.swr_ctx, resampled_frame, ds.frame);
                av_audio_fifo_write(ds.fifo, (void **)resampled_frame->data, resampled_frame->nb_samples);
                av_frame_free(&resampled_frame);

                printf("Audio decoding...\n");
            }
        }   
    }
    pthread_mutex_unlock(&ds.queue_mutex);
    return 0;
}
void decoder_stop(void)
{
    ds.decoding = false;
    printf("waiting for thread to finish...\n");
    pthread_join(ds.decode_thread, NULL);
    // pthread_join(ds.video_thread, NULL);
    pthread_mutex_destroy(&ds.queue_mutex);

    queue_free(ds.video_queue);
    queue_free(ds.audio_queue);

    av_frame_free(&ds.frame);
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
