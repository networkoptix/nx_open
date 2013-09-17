/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#include "third_party_archive_delegate.h"

#include <plugins/plugin_tools.h>

#include "third_party_stream_reader.h"


static QAtomicInt ThirdPartyArchiveDelegate_count = 0;

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

    ThirdPartyArchiveDelegate_count.fetchAndAddOrdered( 1 );
}

ThirdPartyArchiveDelegate::~ThirdPartyArchiveDelegate()
{
    if( m_streamReader )
        m_streamReader->releaseRef();
    m_archiveReader->releaseRef();

    ThirdPartyArchiveDelegate_count.fetchAndAddOrdered( -1 );
}

bool ThirdPartyArchiveDelegate::open( QnResourcePtr resource )
{
    if( m_resource != resource )
        return false;
    if( !m_archiveReader->open() )
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

qint64 ThirdPartyArchiveDelegate::startTime()
{
    return m_archiveReader->startTime();
}

qint64 ThirdPartyArchiveDelegate::endTime()
{
    return m_archiveReader->endTime();
}

QnAbstractMediaDataPtr ThirdPartyArchiveDelegate::getNextData()
{
    if( !m_streamReader )
        return QnAbstractMediaDataPtr(0);

    QnAbstractMediaDataPtr rez;

    if( m_savedMediaPacket )
    {
        rez = m_savedMediaPacket;
        m_savedMediaPacket = QnAbstractMediaDataPtr();
    }
    else
    {
        rez = ThirdPartyStreamReader::readStreamReader( m_streamReader );
        if( rez )
        {
            QnCompressedVideoDataPtr videoData = rez.dynamicCast<QnCompressedVideoData>();
            if( videoData && videoData->motion )
            {
                rez = videoData->motion;
                videoData->motion = QnMetaDataV1Ptr();
                m_savedMediaPacket = videoData;
            }
        }
    }

    return rez;
}

qint64 ThirdPartyArchiveDelegate::seek( qint64 time, bool findIFrame )
{
    nxcip::UsecUTCTimestamp selectedPosition = 0;
    switch( m_archiveReader->seek( time, findIFrame, &selectedPosition, &m_cSeq ) )
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

static QnDefaultResourceVideoLayout videoLayout;
QnResourceVideoLayout* ThirdPartyArchiveDelegate::getVideoLayout()
{
    return &videoLayout;
}

static QnEmptyResourceAudioLayout audioLayout;
QnResourceAudioLayout* ThirdPartyArchiveDelegate::getAudioLayout()
{
    return &audioLayout;
}

void ThirdPartyArchiveDelegate::onReverseMode( qint64 displayTime, bool value )
{
    nxcip::UsecUTCTimestamp actualSelectedTimestamp = nxcip::INVALID_TIMESTAMP_VALUE;
    m_archiveReader->setReverseMode(
        value,
        (displayTime == 0 || displayTime == AV_NOPTS_VALUE)
            ? nxcip::INVALID_TIMESTAMP_VALUE
            : displayTime,
        &actualSelectedTimestamp,
        &m_cSeq );
}

void ThirdPartyArchiveDelegate::setSingleshotMode( bool /*value*/ )
{
    //TODO/IMPL
}

bool ThirdPartyArchiveDelegate::setQuality( MediaQuality quality, bool fastSwitch )
{
    return m_archiveReader->setQuality(
        quality == MEDIA_Quality_High || quality == MEDIA_Quality_ForceHigh
            ? nxcip::msqHigh
            : (quality == MEDIA_Quality_Low ? nxcip::msqLow : nxcip::msqDefault),
        fastSwitch ) == nxcip::NX_IO_ERROR;
}

void ThirdPartyArchiveDelegate::setRange( qint64 startTime, qint64 endTime, qint64 frameStep )
{
    m_archiveReader->playRange( startTime, endTime, frameStep, &m_cSeq );
}

void ThirdPartyArchiveDelegate::setSendMotion( bool value )
{
    m_archiveReader->setMotionDataEnabled( value );
}
