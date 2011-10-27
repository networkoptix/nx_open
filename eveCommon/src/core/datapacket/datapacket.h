#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include <QSharedPointer>
#include "../base/threadqueue.h"

class QnAbstractStreamDataProvider;

struct QnAbstractDataPacket
{
    virtual ~QnAbstractDataPacket() {}
	QnAbstractStreamDataProvider* dataProvider;
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;
typedef CLThreadQueue<QnAbstractDataPacketPtr> CLDataQueue;

#endif //abstract_data_h_1112
