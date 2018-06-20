#pragma once

#include <memory>

#include "hls_playlist_manager.h"


namespace nx_hls {

//!Redirects calls to another AbstractPlaylistManager. If target object has been removed, does nothing
class HlsPlayListManagerWeakRefProxy:
    public AbstractPlaylistManager
{
public:
    HlsPlayListManagerWeakRefProxy(AbstractPlaylistManagerPtr target);

    virtual size_t generateChunkList(
        std::vector<ChunkData>* const chunkList,
        bool* const endOfStreamReached) const override;
    virtual int getMaxBitrate() const override;

private:
    std::weak_ptr<AbstractPlaylistManager> m_target;
};

}
