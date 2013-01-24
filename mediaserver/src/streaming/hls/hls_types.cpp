////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_types.h"


namespace nx_hls
{
    Chunk::Chunk()
    :
        duration( 0 )
    {
    }

    Playlist::Playlist()
    :
        mediaSequence( 0 ),
        closed( false )
    {
    }

    QByteArray Playlist::toString() const
    {
        //TODO/IMPL
        return QByteArray();
    }
}
