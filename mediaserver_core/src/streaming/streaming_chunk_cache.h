#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QCache>
#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>

#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"
#include "streaming_chunk_provider.h"

class QnResourcePool;
class StreamingChunkTranscoder;

class StreamingChunkCache:
    public QObject
{
    Q_OBJECT

public:
    StreamingChunkCache(
        QnResourcePool* resourcePool,
        StreamingChunkTranscoder* transcoder,
        std::chrono::seconds cacheSize);

    std::unique_ptr<StreamingChunkInputStream> getChunkForReading(
        const StreamingChunkCacheKey currentChunkKey,
        StreamingChunkPtr* chunk);

private:
    mutable QnMutex m_mutex;
    QCache<StreamingChunkCacheKey, StreamingChunkPtr> m_cache;
    std::unique_ptr<AbstractStreamingChunkProvider> m_streamingChunkProvider;
};
