////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "live_media_cache_reader.h"


LiveMediaCacheReader::LiveMediaCacheReader( const MediaStreamCache* mediaCache )
:
    m_mediaCache( mediaCache )
{
    Q_ASSERT( false );
}

bool LiveMediaCacheReader::tryRead( QnAbstractDataPacketPtr* const data )
{
    //TODO/IMPL
    return false;
}
