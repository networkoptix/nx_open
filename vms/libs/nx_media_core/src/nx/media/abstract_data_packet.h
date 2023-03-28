// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/datetime.h>

class QnAbstractStreamDataProvider;

struct NX_MEDIA_CORE_API QnAbstractDataPacket
{
    virtual ~QnAbstractDataPacket() = default;

    qint64 timestamp = DATETIME_INVALID; //< TODO: Rename to timestampUs.

    bool isSpecialTimeValue() const
    {
        return timestamp < 0 || timestamp == DATETIME_NOW;
    }
};

typedef std::shared_ptr<QnAbstractDataPacket> QnAbstractDataPacketPtr;
typedef std::shared_ptr<const QnAbstractDataPacket> QnConstAbstractDataPacketPtr;
