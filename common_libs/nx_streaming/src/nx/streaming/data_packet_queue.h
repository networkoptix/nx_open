#ifndef DATA_PACKET_QUEUE_H
#define DATA_PACKET_QUEUE_H

#include <utils/common/threadqueue.h>
#include <nx/streaming/abstract_data_packet.h>

typedef CLThreadQueue<QnAbstractDataPacketPtr> QnDataPacketQueue;
typedef CLThreadQueue<QnConstAbstractDataPacketPtr> QnConstDataPacketQueue;

#endif // DATA_PACKET_QUEUE_H
