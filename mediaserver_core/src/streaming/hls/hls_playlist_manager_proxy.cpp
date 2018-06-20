#include "hls_playlist_manager_proxy.h"


namespace nx_hls {

HlsPlayListManagerWeakRefProxy::HlsPlayListManagerWeakRefProxy(
    AbstractPlaylistManagerPtr target)
    :
    m_target(std::move(target))
{
}

size_t HlsPlayListManagerWeakRefProxy::generateChunkList(
    std::vector<ChunkData>* const chunkList,
    bool* const endOfStreamReached) const
{
    auto strongTargetRef = m_target.lock();
    if (!strongTargetRef)
        return 0;

    return strongTargetRef->generateChunkList(chunkList, endOfStreamReached);
}

int HlsPlayListManagerWeakRefProxy::getMaxBitrate() const
{
    auto strongTargetRef = m_target.lock();
    if (!strongTargetRef)
        return 0;

    return strongTargetRef->getMaxBitrate();
}

}
