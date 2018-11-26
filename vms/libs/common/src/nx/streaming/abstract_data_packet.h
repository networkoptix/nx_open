#pragma once

#include <memory>

#include <QtCore/QMetaType>

#include <nx/utils/datetime.h>

class QnAbstractStreamDataProvider;

struct QnAbstractDataPacket
{
    virtual ~QnAbstractDataPacket() = default;

    qint64 timestamp = DATETIME_INVALID; //< TODO: Rename to timestampUs.
};

typedef std::shared_ptr<QnAbstractDataPacket> QnAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnAbstractDataPacketPtr)
typedef std::shared_ptr<const QnAbstractDataPacket> QnConstAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnConstAbstractDataPacketPtr)
