#ifndef QUEUE_H
#define QUEUE_H


#include <stdio.h>
#include <stdlib.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>

#include "decoder.h"
#include "player.h"

typedef struct PacketQueue
{
    AVPacket *packet;
    struct PacketQueue *next;
} PacketQueue;

typedef struct Queue
{
    PacketQueue *head;
    PacketQueue *tail;
} Queue;

Queue *queue_init(void);
void queue_push(Queue *queue, AVPacket *packet);
AVPacket *queue_pop(Queue *queue);
void queue_free(Queue *queue);
void queue_clear(Queue *queue);
#endif // QUEUE_H
