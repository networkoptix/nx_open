#pragma once

#include <map>

#include "abstract_motion_archive.h"

/**
 * NOTE: All methods are not thread-safe.
 */
class MetadataMultiplexer:
    public QnAbstractMotionArchiveConnection
{
public:
    /**
     * @return Packet with minimal timestamp from any reader.
     * If no reader produces a packet, then null is returned.
     */
    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) override;

    /**
     * NOTE: If id already taken, object is replaced.
     */
    void add(int id, QnAbstractMotionArchiveConnectionPtr metadataReader);
    QnAbstractMotionArchiveConnectionPtr readerById(int id);
    void removeById(int id);

private:
    struct ReaderContext
    {
        QnAbstractMotionArchiveConnectionPtr reader;
        QnAbstractCompressedMetadataPtr packet;
    };

    std::map<int /*id*/, ReaderContext> m_readers;
};
