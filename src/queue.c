#include "queue.h"

Queue *queue_init(void) {
    Queue *ret = malloc(sizeof(Queue));
    if (!ret) {
        fprintf(stderr, "ERROR: Could not allocate video queue\n");
        return NULL;
    }
    ret->head = NULL;
    ret->tail = NULL;
    return ret;
}
void queue_push(Queue *queue, AVPacket *packet) {
    PacketQueue *item = malloc(sizeof(PacketQueue));
    item->packet = av_packet_alloc();
    av_packet_move_ref(item->packet, packet);
    item->next = NULL;
    if (queue->tail)
        queue->tail->next = item;
    else
        queue->head = item;
    queue->tail = item;
}

AVPacket *queue_pop(Queue *queue) {
    if (!queue->head)
        return NULL;
    PacketQueue *item = queue->head;
    AVPacket *packet = item->packet;
    queue->head = item->next;
    if (!queue->head)
        queue->tail = NULL;
    free(item);
    return packet;
}
void queue_free(Queue *queue) {
    PacketQueue *current = queue->head;
    while (current) {
        PacketQueue *next = current->next;
        av_packet_free(&current->packet);
        free(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
}

void queue_clear(Queue *queue) {
    if (!queue)
        return;

    PacketQueue *current = queue->head;
    while (current) {
        PacketQueue *next = current->next;
        av_packet_free(&current->packet);
        free(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
}
