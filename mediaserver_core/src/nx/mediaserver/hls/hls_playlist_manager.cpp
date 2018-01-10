#include "hls_playlist_manager.h"

namespace nx {
namespace mediaserver {
namespace hls {

AbstractPlaylistManager::ChunkData::ChunkData()
:
    startTimestamp( 0 ),
    duration( 0 ),
    mediaSequence( 0 ),
    discontinuity( false )
{
}

AbstractPlaylistManager::ChunkData::ChunkData(
    quint64 _startTimestamp,
    quint64 _duration,
    unsigned int _mediaSequence,
    bool _discontinuity )
:
    startTimestamp( _startTimestamp ),
    duration( _duration ),
    mediaSequence( _mediaSequence ),
    discontinuity( _discontinuity )
{
}

AbstractPlaylistManager::ChunkData::ChunkData(
    const QString& _alias,
    unsigned int _mediaSequence,
    bool _discontinuity )
:
    alias( _alias ),
    startTimestamp( 0 ),
    duration( 0 ),
    mediaSequence( _mediaSequence ),
    discontinuity( _discontinuity )
{
}

} // namespace hls
} // namespace mediaserver
} // namespace nx
