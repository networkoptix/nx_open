#pragma once

#include <vector>

#include "abstract_motion_archive.h"

class MetadataMultiplexer:
    public QnAbstractMotionArchiveConnection
{
public:
    /**
     * @return Packet with minimal timestamp from any reader.
     * If no reader produces a packet, then null is returned.
     */
    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) override;

    void add(QnAbstractMotionArchiveConnectionPtr metadataReader);

private:
    struct ReaderContext
    {
        QnAbstractMotionArchiveConnectionPtr reader;
        QnAbstractCompressedMetadataPtr packet;
    };

    std::vector<ReaderContext> m_readers;
};
