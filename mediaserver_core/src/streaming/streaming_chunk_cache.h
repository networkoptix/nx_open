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
        const nx::mediaserver::Settings* settings,
        QnResourcePool* resourcePool,
        StreamingChunkTranscoder* transcoder);

    std::unique_ptr<StreamingChunkInputStream> getChunkForReading(
        const StreamingChunkCacheKey currentChunkKey,
        StreamingChunkPtr* chunk);

private:
    mutable QnMutex m_mutex;
    const nx::mediaserver::Settings* m_settings;
    QCache<StreamingChunkCacheKey, StreamingChunkPtr> m_cache;
    std::unique_ptr<AbstractStreamingChunkProvider> m_streamingChunkProvider;
};
