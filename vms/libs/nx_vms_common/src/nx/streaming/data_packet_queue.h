// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef DATA_PACKET_QUEUE_H
#define DATA_PACKET_QUEUE_H

#include <utils/common/threadqueue.h>
#include <nx/streaming/abstract_data_packet.h>

typedef QnSafeQueue<QnAbstractDataPacketPtr> QnDataPacketQueue;
typedef QnSafeQueue<QnConstAbstractDataPacketPtr> QnConstDataPacketQueue;

#endif // DATA_PACKET_QUEUE_H
