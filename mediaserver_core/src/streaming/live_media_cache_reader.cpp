#include "live_media_cache_reader.h"


LiveMediaCacheReader::LiveMediaCacheReader( MediaStreamCache* mediaCache, quint64 startTimestamp )
:
    m_readCtx( mediaCache, startTimestamp )
{
}

bool LiveMediaCacheReader::tryRead( QnAbstractDataPacketPtr* const data )
{
    //TODO/HLS #ak throw dataAvailable signal if data is available after returning false
    if (!m_frontStack.empty())
    {
        *data = std::move(m_frontStack.top());
        m_frontStack.pop();
        return true;
    }

    QnAbstractDataPacketPtr packet = m_readCtx.getNextFrame();
    if( !packet )
        return false;

    *data = packet;
    return true;
}

quint64 LiveMediaCacheReader::currentPos() const
{
    if (!m_frontStack.empty())
        return m_frontStack.top()->timestamp;

    return m_readCtx.currentPos();
}

void LiveMediaCacheReader::put(QnAbstractDataPacketPtr packet)
{
    m_frontStack.push(std::move(packet));
}
