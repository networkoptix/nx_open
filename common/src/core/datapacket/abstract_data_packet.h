#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include <QtCore/QSharedPointer>

#include "abstract_media_context.h"

class QnAbstractStreamDataProvider;

struct QnAbstractDataPacket {
    QnAbstractDataPacket(): dataProvider(0), timestamp(DATETIME_INVALID) {} 
    virtual ~QnAbstractDataPacket() {}

    QnAbstractStreamDataProvider *dataProvider;
    qint64 timestamp; // mksec // 10^-6
};

typedef QSharedPointer<QnAbstractDataPacket> QnAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnAbstractDataPacketPtr);
typedef QSharedPointer<const QnAbstractDataPacket> QnConstAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnConstAbstractDataPacketPtr);

#endif //abstract_data_h_1112
