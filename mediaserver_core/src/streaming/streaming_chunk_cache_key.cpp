////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_cache_key.h"

#include <QHash>
#include <QStringList>

#include "streaming_params.h"


StreamingChunkCacheKey::StreamingChunkCacheKey()
:
    m_channel( 0 ),
    m_startTimestamp( 0 ),
    m_duration( 0 )
{
}

/*!
    \param channel channel number or -1 for all channels
    \param containerFormat E.g., ts for mpeg2/ts
    \param startTimestamp
    \param endTimestamp
    \param auxiliaryParams
*/
StreamingChunkCacheKey::StreamingChunkCacheKey(
    const QString& uniqueResourceID,
    int channel,
    const QString& containerFormat,
    const QString& alias,
    quint64 startTimestamp,
    std::chrono::microseconds duration,
    MediaQuality streamQuality,
    const std::multimap<QString, QString>& auxiliaryParams )
:
    m_uniqueResourceID( uniqueResourceID ),
    m_channel( channel ),
    m_containerFormat( containerFormat ),
    m_alias( alias ),
    m_startTimestamp( startTimestamp ),
    m_duration( duration ),
    m_streamQuality( streamQuality ),
    m_isLive( false ),
    m_auxiliaryParams( auxiliaryParams )
{
    NX_ASSERT( !containerFormat.isEmpty() );

    std::multimap<QString, QString>::const_iterator it = auxiliaryParams.find( StreamingParams::VIDEO_CODEC_PARAM_NAME );
    if( it != auxiliaryParams.end() )
        m_videoCodec = it->second;

    it = auxiliaryParams.find( StreamingParams::AUDIO_CODEC_PARAM_NAME );
    if( it != auxiliaryParams.end() )
        m_audioCodec = it->second;

    it = auxiliaryParams.find( StreamingParams::PICTURE_SIZE_PIXELS_PARAM_NAME );
    if( it != auxiliaryParams.end() )
    {
        //1920x1080
        const QStringList& sizesStr = it->second.split(QChar('x'));
        m_pictureSizePixels = QSize(
            sizesStr.size() > 0 ? sizesStr[0].toInt() : 0,
            sizesStr.size() > 1 ? sizesStr[1].toInt() : 0 );
    }

    it = auxiliaryParams.find( StreamingParams::LIVE_PARAM_NAME );
    m_isLive = it != auxiliaryParams.end();
}

StreamingChunkCacheKey::StreamingChunkCacheKey( const QString& uniqueResourceID )
:
    m_uniqueResourceID( uniqueResourceID ),
    m_channel( 0 ),
    m_startTimestamp( 0 ),
    m_duration( 0 )
{
}

//!data source (camera id, stream id)
const QString& StreamingChunkCacheKey::srcResourceUniqueID() const
{
    return m_uniqueResourceID;
}

unsigned int StreamingChunkCacheKey::channel() const
{
    return m_channel;
}

QString StreamingChunkCacheKey::alias() const
{
    return m_alias;
}

quint64 StreamingChunkCacheKey::startTimestamp() const
{
    return m_startTimestamp;
}

std::chrono::microseconds StreamingChunkCacheKey::duration() const
{
    return m_duration;
}

//!startTimestamp() + duration
quint64 StreamingChunkCacheKey::endTimestamp() const
{
    return m_startTimestamp + m_duration.count();
}

MediaQuality StreamingChunkCacheKey::streamQuality() const
{
    return m_streamQuality;
}

//!Video resolution
const QSize& StreamingChunkCacheKey::pictureSizePixels() const
{
    return m_pictureSizePixels;
}

//media format (codec format, container format)
const QString& StreamingChunkCacheKey::containerFormat() const
{
    return m_containerFormat;
}

const QString& StreamingChunkCacheKey::videoCodec() const
{
    return m_videoCodec;
}

const QString& StreamingChunkCacheKey::audioCodec() const
{
    return m_audioCodec;
}

bool StreamingChunkCacheKey::live() const
{
    return m_isLive;
}

QString StreamingChunkCacheKey::streamingSessionId() const
{
    auto it = m_auxiliaryParams.find(StreamingParams::SESSION_ID_PARAM_NAME);
    if (it != m_auxiliaryParams.end())
        return it->second;
    return QString();
}

bool StreamingChunkCacheKey::mediaStreamParamsEqualTo(const StreamingChunkCacheKey& right) const
{
    return srcResourceUniqueID() == right.srcResourceUniqueID() &&
           channel() == right.channel() &&
           streamQuality() == right.streamQuality() &&
           pictureSizePixels() == right.pictureSizePixels() &&
           containerFormat() == right.containerFormat() &&
           videoCodec() == right.videoCodec() &&
           audioCodec() == right.audioCodec();
}

bool StreamingChunkCacheKey::operator<( const StreamingChunkCacheKey& right ) const
{
    if( m_uniqueResourceID < right.m_uniqueResourceID )
        return true;
    if( m_uniqueResourceID > right.m_uniqueResourceID )
        return false;

    if( m_channel < right.m_channel )
        return true;
    if( m_channel > right.m_channel )
        return false;

    if( m_containerFormat < right.m_containerFormat )
        return true;
    if( m_containerFormat > right.m_containerFormat )
        return false;

    if( m_alias < right.m_alias )
        return true;
    if( m_alias > right.m_alias )
        return false;

    if( m_startTimestamp < right.m_startTimestamp )
        return true;
    if( m_startTimestamp > right.m_startTimestamp )
        return false;

    if( m_duration < right.m_duration )
        return true;
    if( m_duration > right.m_duration )
        return false;

    if( m_pictureSizePixels.width() < right.m_pictureSizePixels.width() )
        return true;
    if( m_pictureSizePixels.width() > right.m_pictureSizePixels.width() )
        return false;

    if( m_pictureSizePixels.height() < right.m_pictureSizePixels.height() )
        return true;
    if( m_pictureSizePixels.height() > right.m_pictureSizePixels.height() )
        return false;

    if( m_videoCodec < right.m_videoCodec )
        return true;
    if( m_videoCodec > right.m_videoCodec )
        return false;

    if( m_audioCodec < right.m_audioCodec )
        return true;
    if( m_audioCodec > right.m_audioCodec )
        return false;

    if( m_streamQuality < right.m_streamQuality )
        return true;
    if( m_streamQuality > right.m_streamQuality )
        return false;

    //if( m_auxiliaryParams < right.m_auxiliaryParams )
    //    return true;
    //else if( m_auxiliaryParams > right.m_auxiliaryParams )
    //    return false;

    return false;   //equal
}

bool StreamingChunkCacheKey::operator>( const StreamingChunkCacheKey& right ) const
{
    return right < *this;
}

bool StreamingChunkCacheKey::operator==( const StreamingChunkCacheKey& right ) const
{
    return (m_uniqueResourceID == right.m_uniqueResourceID)
        && (m_channel == right.m_channel)
        && (m_containerFormat == right.m_containerFormat)
        && (m_alias == right.m_alias)
        && (m_startTimestamp == right.m_startTimestamp)
        && (m_duration == right.m_duration)
        && (m_streamQuality == right.m_streamQuality);
        //&& (m_auxiliaryParams == right.m_auxiliaryParams);
}

bool StreamingChunkCacheKey::operator!=( const StreamingChunkCacheKey& right ) const
{
    return !(*this == right);
}

uint qHash( const StreamingChunkCacheKey& key )
{
    return qHash(key.srcResourceUniqueID())
        + key.channel()
        + key.startTimestamp()
        + qHash(key.alias())
        + key.duration().count()
        + key.endTimestamp()
        + key.streamQuality()
        + key.pictureSizePixels().width()
        + key.pictureSizePixels().height()
        + qHash(key.containerFormat())
        + qHash(key.videoCodec())
        + qHash(key.audioCodec());
}
