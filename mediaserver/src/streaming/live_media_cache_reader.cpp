////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "live_media_cache_reader.h"


LiveMediaCacheReader::LiveMediaCacheReader( MediaStreamCache* mediaCache, quint64 startTimestamp )
:
    m_readCtx( mediaCache, startTimestamp )
{
}

bool LiveMediaCacheReader::tryRead( QnAbstractDataPacketPtr* const data )
{
    //TODO/HLS #ak throw dataAvailable signal if data is available after returning false

    QnAbstractDataPacketPtr packet = m_readCtx.getNextFrame();
    if( !packet )
        return false;

    *data = packet;
    return true;
}

quint64 LiveMediaCacheReader::currentPos() const
{
    return m_readCtx.currentPos();
}
