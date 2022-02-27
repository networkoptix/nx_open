// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QMetaType>

#include <nx/utils/datetime.h>

class QnAbstractStreamDataProvider;

struct NX_VMS_COMMON_API QnAbstractDataPacket
{
    virtual ~QnAbstractDataPacket() = default;

    qint64 timestamp = DATETIME_INVALID; //< TODO: Rename to timestampUs.

    bool isSpecialTimeValue() const
    {
        return timestamp < 0 || timestamp == DATETIME_NOW;
    }
};

typedef std::shared_ptr<QnAbstractDataPacket> QnAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnAbstractDataPacketPtr)
typedef std::shared_ptr<const QnAbstractDataPacket> QnConstAbstractDataPacketPtr;
Q_DECLARE_METATYPE(QnConstAbstractDataPacketPtr)
