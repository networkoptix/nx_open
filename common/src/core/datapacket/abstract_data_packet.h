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

#endif //abstract_data_h_1112
