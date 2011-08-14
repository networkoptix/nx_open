#ifndef abstract_data_packrt_h_1112
#define abstract_data_packrt_h_1112

#include <QtCore/QSharedPointer>

#include "common/threadqueue.h"

class QnAbstractMediaStreamDataProvider;

struct QnAbstractDataPacket
{
    QnAbstractMediaStreamDataProvider* dataProvider;

    virtual ~QnAbstractDataPacket()
    {
    }
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;
typedef CLThreadQueue<QnAbstractDataPacketPtr> QnDataPacketQueue;

#endif //abstract_data_packrt_h_1112
