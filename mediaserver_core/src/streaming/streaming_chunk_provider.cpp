////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_provider.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "media_server/media_server_module.h"
#include "media_server/settings.h"
#include "streaming_chunk_transcoder.h"
#include "video_camera_streaming_chunk.h"

StreamingChunkProvider::StreamingChunkProvider(
    QnResourcePool* resourcePool,
    StreamingChunkTranscoder* transcoder)
    :
    m_resourcePool(resourcePool),
    m_transcoder(transcoder)
{
}

bool StreamingChunkProvider::get(
    const StreamingChunkCacheKey& key,
    int* const itemCost,
    StreamingChunkPtr* const chunk)
{
    using namespace std::chrono;

    StreamingChunkPtr newChunk = std::make_shared<VideoCameraStreamingChunk>(
        m_resourcePool,
        key,
        qnServerModule->settings()->roSettings()->value(
            nx_ms_conf::HLS_MAX_CHUNK_BUFFER_SIZE,
            nx_ms_conf::DEFAULT_HLS_MAX_CHUNK_BUFFER_SIZE).toUInt());
    if( !m_transcoder->transcodeAsync( key, newChunk ) )
        return false;

    //NOTE at this time chunk size in bytes is unknown, since transcoding is about to be started
    *itemCost = duration_cast<seconds>(key.duration()).count();
    *chunk = newChunk;
    return true;
}

//-------------------------------------------------------------------------------------------------

std::unique_ptr<AbstractStreamingChunkProvider> StreamingChunkProviderFactory::create(
    QnResourcePool* resourcePool,
    StreamingChunkTranscoder* transcoder)
{
    if (m_customFunc)
        return m_customFunc();

    return std::make_unique<StreamingChunkProvider>(resourcePool, transcoder);
}

StreamingChunkProviderFactory::Function StreamingChunkProviderFactory::setCustomFunc(
    Function customFunc)
{
    m_customFunc.swap(customFunc);
    return customFunc;
}

StreamingChunkProviderFactory& StreamingChunkProviderFactory::instance()
{
    static StreamingChunkProviderFactory staticInstance;
    return staticInstance;
}
