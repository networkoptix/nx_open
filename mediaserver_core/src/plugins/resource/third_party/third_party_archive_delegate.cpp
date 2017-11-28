/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#include "third_party_archive_delegate.h"

#ifdef ENABLE_THIRD_PARTY

#include <plugins/plugin_tools.h>

#include "third_party_stream_reader.h"


ThirdPartyArchiveDelegate::ThirdPartyArchiveDelegate(
    const QnResourcePtr& resource,
    nxcip::DtsArchiveReader* archiveReader )
:
    m_resource( resource ),
    m_archiveReader( archiveReader ),
    m_streamReader( NULL ),
    m_cSeq( 0 )
{
    unsigned int caps = m_archiveReader->getCapabilities();
    if( caps & nxcip::DtsArchiveReader::reverseGopModeCapability )
        m_flags |= QnAbstractArchiveDelegate::Flag_CanProcessNegativeSpeed;
    if( caps & nxcip::DtsArchiveReader::motionDataCapability )
        m_flags |= QnAbstractArchiveDelegate::Flag_CanSendMotion;

    m_flags |= QnAbstractArchiveDelegate::Flag_CanOfflineRange;
    m_flags |= QnAbstractArchiveDelegate::Flag_CanSeekImmediatly;
    m_flags |= QnAbstractArchiveDelegate::Flag_CanOfflineLayout;
    m_flags |= QnAbstractArchiveDelegate::Flag_UnsyncTime;
    m_flags |= QnAbstractArchiveDelegate::Flag_CanOfflineHasVideo;
}

ThirdPartyArchiveDelegate::~ThirdPartyArchiveDelegate()
{
    if( m_streamReader )
        m_streamReader->releaseRef();
    m_archiveReader->releaseRef();
}

bool ThirdPartyArchiveDelegate::open(const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher * /*archiveIntegrityWatcher*/)
{
    if( m_resource != resource )
        return false;
    if( m_archiveReader->open() != nxcip::NX_NO_ERROR )
        return false;
    if( m_streamReader )
        m_streamReader->releaseRef();
    m_streamReader = m_archiveReader->getStreamReader();
    if( !m_streamReader )
        return false;

    return true;
}

void ThirdPartyArchiveDelegate::close()
{
    if( m_streamReader )
    {
        m_streamReader->releaseRef();
        m_streamReader = NULL;
    }
}

qint64 ThirdPartyArchiveDelegate::startTime() const
{
    return m_archiveReader->startTime();
}

qint64 ThirdPartyArchiveDelegate::endTime() const
{
    return m_archiveReader->endTime();
}

QnAbstractMediaDataPtr ThirdPartyArchiveDelegate::getNextData()
{
    if( !m_streamReader )
        return QnAbstractMediaDataPtr(0);

    QnAbstractMediaDataPtr rez;

    if( m_savedMediaPacket )
        rez = m_savedMediaPacket;
    else
        rez = ThirdPartyStreamReader::readStreamReader(m_streamReader);
    m_savedMediaPacket = QnAbstractMediaDataPtr();

    if( rez )
    {
        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(rez);
        if( videoData && !videoData->metadata.isEmpty())
        {
            rez = videoData->metadata.first();
            videoData->metadata.pop_front();
            m_savedMediaPacket = videoData;
        }
    }

    return rez;
}

qint64 ThirdPartyArchiveDelegate::seek( qint64 time, bool findIFrame )
{
    nxcip::UsecUTCTimestamp selectedPosition = 0;
    switch( m_archiveReader->seek( ++m_cSeq, time, findIFrame, &selectedPosition ) )
    {
        case nxcip::NX_NO_ERROR:
            return selectedPosition;

        case nxcip::NX_NO_DATA:
            //returning zero in case of reverse play, DATETIME_NOW in case of forward play
            return m_archiveReader->isReverseModeEnabled() ? 0 : DATETIME_NOW;

        default:
            return AV_NOPTS_VALUE;
    }
}

static QSharedPointer<QnDefaultResourceVideoLayout> videoLayout( new QnDefaultResourceVideoLayout() );
QnConstResourceVideoLayoutPtr ThirdPartyArchiveDelegate::getVideoLayout()
{
    return videoLayout;
}

static QSharedPointer<QnEmptyResourceAudioLayout> audioLayout( new QnEmptyResourceAudioLayout() );
QnConstResourceAudioLayoutPtr ThirdPartyArchiveDelegate::getAudioLayout()
{
    return audioLayout;
}

void ThirdPartyArchiveDelegate::setSpeed( qint64 displayTime, double value )
{
    nxcip::UsecUTCTimestamp actualSelectedTimestamp = nxcip::INVALID_TIMESTAMP_VALUE;
    m_archiveReader->setReverseMode(
        ++m_cSeq,
        value < 0,
        (displayTime == 0 || displayTime == (qint64)AV_NOPTS_VALUE)
            ? nxcip::INVALID_TIMESTAMP_VALUE
            : displayTime,
        &actualSelectedTimestamp );
}

void ThirdPartyArchiveDelegate::setSingleshotMode( bool /*value*/ )
{
    //TODO/IMPL
}

bool ThirdPartyArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution)
{
    Q_UNUSED(resolution); //< not implemented
    return m_archiveReader->setQuality(
        quality == MEDIA_Quality_High || quality == MEDIA_Quality_ForceHigh
            ? nxcip::msqHigh
            : (quality == MEDIA_Quality_Low ? nxcip::msqLow : nxcip::msqDefault),
        fastSwitch ) == nxcip::NX_IO_ERROR;
}

void ThirdPartyArchiveDelegate::setRange( qint64 startTime, qint64 endTime, qint64 frameStep )
{
    m_archiveReader->playRange( ++m_cSeq, startTime, endTime, frameStep );
}

void ThirdPartyArchiveDelegate::setSendMotion( bool value )
{
    m_archiveReader->setMotionDataEnabled( value );
}

#endif // ENABLE_THIRD_PARTY
