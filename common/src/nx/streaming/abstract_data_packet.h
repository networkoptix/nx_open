#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include <memory>

#include <nx/utils/datetime.h>

class QnAbstractStreamDataProvider;

struct QnAbstractDataPacket {
    QnAbstractDataPacket(): dataProvider(0), timestamp(DATETIME_INVALID) {}
    virtual ~QnAbstractDataPacket() {}

    QnAbstractStreamDataProvider *dataProvider;
    qint64 timestamp; // mksec // 10^-6
};

typedef std::shared_ptr<QnAbstractDataPacket> QnAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnAbstractDataPacketPtr);
typedef std::shared_ptr<const QnAbstractDataPacket> QnConstAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnConstAbstractDataPacketPtr);

#endif //abstract_data_h_1112
