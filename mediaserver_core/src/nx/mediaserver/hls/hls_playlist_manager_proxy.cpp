#include "hls_playlist_manager_proxy.h"

namespace nx {
namespace mediaserver {
namespace hls {

PlayListManagerWeakRefProxy::PlayListManagerWeakRefProxy(
    AbstractPlaylistManagerPtr target )
:
    m_target( std::move(target) )
{
}

size_t PlayListManagerWeakRefProxy::generateChunkList(
    std::vector<ChunkData>* const chunkList,
    bool* const endOfStreamReached ) const
{
    auto strongTargetRef = m_target.lock();
    if( !strongTargetRef )
        return 0;

    return strongTargetRef->generateChunkList( chunkList, endOfStreamReached );
}

int PlayListManagerWeakRefProxy::getMaxBitrate() const
{
    auto strongTargetRef = m_target.lock();
    if( !strongTargetRef )
        return 0;

    return strongTargetRef->getMaxBitrate();
}

} // namespace hls
} // namespace mediaserver
} // namespace nx
