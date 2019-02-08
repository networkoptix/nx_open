#pragma once

#include <memory>

#include "nx/streaming/media_data_packet.h"

class QnAbstractMotionArchiveConnection
{
public:
    virtual ~QnAbstractMotionArchiveConnection() = default;

    /**
     * @return metadata in motion archive for specified time.
     * If next call got same metadata or no metada is found return null.
     */
    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) = 0;
};
using QnAbstractMotionArchiveConnectionPtr = std::shared_ptr<QnAbstractMotionArchiveConnection>;

class QnAbstractMotionConnectionFactory
{
public:
    virtual ~QnAbstractMotionConnectionFactory() = default;

    virtual QnAbstractMotionArchiveConnectionPtr createMotionArchiveConnection() = 0;
};

using QnAbstractMotionConnectionFactoryPtr = QSharedPointer<QnAbstractMotionConnectionFactory>;
