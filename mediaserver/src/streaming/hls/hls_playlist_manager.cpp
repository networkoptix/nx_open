////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_playlist_manager.h"


namespace nx_hls
{
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
}
