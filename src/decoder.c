#include "decoder.h"
#include "raylib.h"
#include <stdbool.h>
#include <stdlib.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libavutil/dict.h>

#define FIFO_MIN_FRAMES 1024 * 4

DecoderState ds = {0};
int64_t frame_time = 0;
float playback_speed = 1.25;

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

    ds.video_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ds.audio_stream_idx = av_find_best_stream(ds.format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ds.video_stream_idx == -1)
    {
        fprintf(stderr, "ERROR: Could not find video stream\n");
        return -1;
    }
    if (ds.audio_stream_idx == -1)
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

    ds.sws_ctx = sws_getContext(ds.video_codec_ctx->width, ds.video_codec_ctx->height, ds.video_codec_ctx->pix_fmt, ds.video_codec_ctx->width, ds.video_codec_ctx->height, AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);

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

    int ret = swr_alloc_set_opts2(&ds.swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_FLT, ds.audio_codec_ctx->sample_rate, &ds.audio_codec_ctx->ch_layout, ds.audio_codec_ctx->sample_fmt, ds.audio_codec_ctx->sample_rate, 0, NULL);

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
    ds.fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, 2, FIFO_MIN_FRAMES * 2);
    return 0;
}

int decoder_decode_frame(Texture texture, int64_t *frame_time)
{
    int read_bytes = av_read_frame(ds.format_ctx, ds.packet);
    if (read_bytes < 0)
        return -1;

    if (ds.packet->stream_index == ds.audio_stream_idx)
    {
        if (avcodec_send_packet(ds.audio_codec_ctx, ds.packet) == 0)
        {
            while (avcodec_receive_frame(ds.audio_codec_ctx, ds.frame) == 0)
            {
                AVFrame *resampled_frame = av_frame_alloc();
                resampled_frame->sample_rate = ds.audio_codec_ctx->sample_rate;
                AVChannelLayout out_ch_layout;
                av_channel_layout_default(&out_ch_layout, 2);
                av_channel_layout_copy(&resampled_frame->ch_layout, &out_ch_layout);
                resampled_frame->format = AV_SAMPLE_FMT_FLT;
                resampled_frame->nb_samples = ds.frame->nb_samples;

                swr_convert_frame(ds.swr_ctx, resampled_frame, ds.frame);
                av_audio_fifo_write(ds.fifo, (void **)resampled_frame->data, resampled_frame->nb_samples);
                av_frame_free(&resampled_frame);
            }
        }
    }

    if (ds.packet->stream_index == ds.video_stream_idx)
    {
        int ret = avcodec_send_packet(ds.video_codec_ctx, ds.packet);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Error sending packet\n");
            return -1;
        }

        while (ret >= 0)
        {
            ret = avcodec_receive_frame(ds.video_codec_ctx, ds.frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return -1;
            else if (ret < 0)
            {
                fprintf(stderr, "ERROR: Error receiving frame\n");
                return -1;
            }

            uint8_t *rgba_planes[1] = {ds.rgba_frame_buffer};
            int rgba_linesizes[1] = {ds.video_codec_ctx->width * 4};
            sws_scale(ds.sws_ctx, (const uint8_t *const *)ds.frame->data, ds.frame->linesize, 0, ds.video_codec_ctx->height, rgba_planes, rgba_linesizes);
            UpdateTexture(texture, ds.rgba_frame_buffer);
            *frame_time = (int)ds.frame->pts * av_q2d(ds.video_stream->time_base);
        }
    }
    return 0;
}

int decoder_fill_audio_queue(Texture texture, int64_t *frame_time)
{
    while (av_audio_fifo_size(ds.fifo) < FIFO_MIN_FRAMES)
    {
        if (av_read_frame(ds.format_ctx, ds.packet) == 0 && ds.packet->stream_index == ds.audio_stream_idx)
        {
            if (avcodec_send_packet(ds.audio_codec_ctx, ds.packet) != 0)
            {
                fprintf(stderr, "ERROR: Error sending packet\n");
                continue;
            }
            while (avcodec_receive_frame(ds.audio_codec_ctx, ds.frame) == 0)
            {
                AVFrame *resampled_frame = av_frame_alloc();
                resampled_frame->sample_rate = ds.audio_codec_ctx->sample_rate;
                AVChannelLayout out_ch_layout;
                av_channel_layout_default(&out_ch_layout, 2);
                av_channel_layout_copy(&resampled_frame->ch_layout, &out_ch_layout);

                resampled_frame->format = AV_SAMPLE_FMT_FLT;
                resampled_frame->nb_samples = ds.frame->nb_samples;

                swr_convert_frame(ds.swr_ctx, resampled_frame, ds.frame);
                av_audio_fifo_write(ds.fifo, (void **)resampled_frame->data, resampled_frame->nb_samples);
                av_frame_free(&resampled_frame);
            }
        }
        if (ds.packet->stream_index == ds.video_stream_idx)
        {
            int ret = avcodec_send_packet(ds.video_codec_ctx, ds.packet);
            if (ret < 0)
            {
                fprintf(stderr, "ERROR: Error sending packet\n");
                return -1;
            }
    
            while (ret >= 0)
            {
                ret = avcodec_receive_frame(ds.video_codec_ctx, ds.frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    return -1;
                else if (ret < 0)
                {
                    fprintf(stderr, "ERROR: Error receiving frame\n");
                    return -1;
                }
    
                uint8_t *rgba_planes[1] = {ds.rgba_frame_buffer};
                int rgba_linesizes[1] = {ds.video_codec_ctx->width * 4};
                sws_scale(ds.sws_ctx, (const uint8_t *const *)ds.frame->data, ds.frame->linesize, 0, ds.video_codec_ctx->height, rgba_planes, rgba_linesizes);
                UpdateTexture(texture, ds.rgba_frame_buffer);
                *frame_time = (int)ds.frame->pts * av_q2d(ds.video_stream->time_base);
            }
        }
    }
    av_packet_unref(ds.packet);
    return 0;
}
