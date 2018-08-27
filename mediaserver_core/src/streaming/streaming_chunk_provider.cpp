#include "streaming_chunk_provider.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "media_server/settings.h"
#include "streaming_chunk_transcoder.h"
#include "video_camera_streaming_chunk.h"

StreamingChunkProvider::StreamingChunkProvider(
    QnMediaServerModule* serverModule,
    StreamingChunkTranscoder* transcoder)
    :
    m_serverModule(serverModule),
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
        m_serverModule,
        key,
        m_serverModule->settings().hlsMaxChunkBufferSize());
    if( !m_transcoder->transcodeAsync( key, newChunk ) )
        return false;

    // NOTE: At this time chunk size in bytes is unknown,
    // since transcoding is about to be started.
    *itemCost = duration_cast<seconds>(key.duration()).count();
    *chunk = newChunk;
    return true;
}

//-------------------------------------------------------------------------------------------------

std::unique_ptr<AbstractStreamingChunkProvider> StreamingChunkProviderFactory::create(
    QnMediaServerModule* serverModule,
    StreamingChunkTranscoder* transcoder)
{
    if (m_customFunc)
        return m_customFunc();

    return std::make_unique<StreamingChunkProvider>(serverModule, transcoder);
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
