#ifndef DATAPACKET_H
#define DATAPACKET_H

#include <QtCore/QSharedPointer>

#include "utils/common/threadqueue.h"

class QnAbstractMediaStreamDataProvider;

struct QnAbstractDataPacket
{
    virtual ~QnAbstractDataPacket() {}

    QnAbstractMediaStreamDataProvider *dataProvider;
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;
typedef CLThreadQueue<QnAbstractDataPacketPtr> QnDataPacketQueue;

#endif // DATAPACKET_H
