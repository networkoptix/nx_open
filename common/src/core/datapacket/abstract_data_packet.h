#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include <QSharedPointer>
#include "utils/common/threadqueue.h"


class QnAbstractStreamDataProvider;

struct QnAbstractDataPacket
{
    QnAbstractDataPacket(): dataProvider(0), timestamp(0x8000000000000000ll) {}
    virtual ~QnAbstractDataPacket() {}
    QnAbstractStreamDataProvider* dataProvider;
    qint64 timestamp; // mksec // 10^-6
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;

class CLDataQueue: public CLThreadQueue<QnAbstractDataPacketPtr> 
{
public:
    CLDataQueue(int size): CLThreadQueue<QnAbstractDataPacketPtr> (size) {}
};


#endif //abstract_data_h_1112
