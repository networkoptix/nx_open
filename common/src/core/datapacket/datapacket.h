#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include <QSharedPointer>
#include "utils/common/threadqueue.h"


class QnAbstractStreamDataProvider;

struct QnAbstractDataPacket
{
    QnAbstractDataPacket(): dataProvider(0), timestamp(AV_NOPTS_VALUE) {}
    virtual ~QnAbstractDataPacket() {}
    QnAbstractStreamDataProvider* dataProvider;
    qint64 timestamp; // mksec // 10^-6
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;

class CLDataQueue: public CLThreadQueue<QnAbstractDataPacketPtr> 
{
public:
    CLDataQueue(int size): CLThreadQueue<QnAbstractDataPacketPtr> (size) {}

    qint64 mediaLength() const
    {
        QMutexLocker mutex(&m_cs);
        if (m_queue.isEmpty())
            return 0;
        else
            return m_queue.last()->timestamp - m_queue.front()->timestamp;
    }
};

#endif //abstract_data_h_1112
