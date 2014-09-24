#ifndef QN_DATA_QUEUE_H
#define QN_DATA_QUEUE_H

#include <utils/common/threadqueue.h>

#include "abstract_data_packet.h"

class CLDataQueue: public CLThreadQueue<QnAbstractDataPacketPtr> 
{
public:
    CLDataQueue(int size): CLThreadQueue<QnAbstractDataPacketPtr> (size) {}
};

class CLConstDataQueue: public CLThreadQueue<QnConstAbstractDataPacketPtr> 
{
public:
    CLConstDataQueue(int size): CLThreadQueue<QnConstAbstractDataPacketPtr> (size) {}
};


#endif // QN_DATA_QUEUE_H
