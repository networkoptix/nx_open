#include "streaming_chunk_cache.h"

#include <nx/utils/std/cpp14.h>

#include "media_server/settings.h"
#include <media_server/media_server_module.h>

//TODO/HLS: #ak cache cost MUST be measured in bytes, not seconds!

StreamingChunkCache::StreamingChunkCache(
    QnMediaServerModule* serverModule,
    StreamingChunkTranscoder* transcoder)
    :
    m_serverModule(serverModule),
    m_cache(std::chrono::seconds(serverModule->settings().hlsChunkCacheSizeSec()).count()),
    m_streamingChunkProvider(
        StreamingChunkProviderFactory::instance().create(serverModule, transcoder))
{
}

std::unique_ptr<StreamingChunkInputStream> StreamingChunkCache::getChunkForReading(
    const StreamingChunkCacheKey currentChunkKey,
    StreamingChunkPtr* chunk)
{
    QnMutexLocker lock(&m_mutex);

    StreamingChunkPtr* existingChunk = m_cache.object(currentChunkKey);
    if (existingChunk && (*existingChunk)->startsWithZeroOffset())
    {
        *chunk = *existingChunk;
        return std::make_unique<StreamingChunkInputStream>(chunk->get());
    }

    if (existingChunk)
    {
        // Chunk is useless, removing it...
        m_cache.remove(currentChunkKey);
        existingChunk = nullptr;
    }

    // Creating new chunk.
    int chunkCostSeconds = 1;
    if (!m_streamingChunkProvider->get(currentChunkKey, &chunkCostSeconds, chunk))
        return nullptr;

    NX_ASSERT(*chunk != nullptr);
    m_cache.insert(currentChunkKey, new StreamingChunkPtr(*chunk), chunkCostSeconds);

    return std::make_unique<StreamingChunkInputStream>(chunk->get());
}
