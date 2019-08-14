#include "hls_playlist_manager_proxy.h"

namespace nx {
namespace vms::server {
namespace hls {

PlayListManagerWeakRefProxy::PlayListManagerWeakRefProxy(
    AbstractPlaylistManagerPtr target)
    :
    m_target(std::move(target))
{
}

CameraDiagnostics::Result PlayListManagerWeakRefProxy::generateChunkList(
    std::vector<ChunkData>* const chunkList,
    bool* const endOfStreamReached) const
{
    auto strongTargetRef = m_target.lock();
    if (!strongTargetRef)
        return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::internalServerError);

    return strongTargetRef->generateChunkList(chunkList, endOfStreamReached);
}

int PlayListManagerWeakRefProxy::getMaxBitrate() const
{
    auto strongTargetRef = m_target.lock();
    if (!strongTargetRef)
        return 0;

    return strongTargetRef->getMaxBitrate();
}

} // namespace hls
} // namespace vms::server
} // namespace nx
