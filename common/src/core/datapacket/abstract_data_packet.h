#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include <QSharedPointer>
#include <QSharedPointer>
#include "utils/common/threadqueue.h"


class QnAbstractStreamDataProvider;

class QnAbstractMediaContext
{
public:
    QnAbstractMediaContext() {}
    virtual ~QnAbstractMediaContext() {}
};
typedef QSharedPointer<QnAbstractMediaContext> QnAbstractMediaContextPtr;


struct QnAbstractDataPacket
{
    QnAbstractDataPacket(): dataProvider(0), timestamp(0x8000000000000000ll) {}
    virtual ~QnAbstractDataPacket() {}
    QnAbstractStreamDataProvider* dataProvider;
    qint64 timestamp; // mksec // 10^-6
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnAbstractDataPacketPtr);
typedef QSharedPointer<const QnAbstractDataPacket> QnConstAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnConstAbstractDataPacketPtr);

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

#endif //abstract_data_h_1112
