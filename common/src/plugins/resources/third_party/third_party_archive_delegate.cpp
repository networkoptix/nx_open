/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#include "third_party_archive_delegate.h"

#include <plugins/plugin_tools.h>

#include "third_party_stream_reader.h"


ThirdPartyArchiveDelegate::ThirdPartyArchiveDelegate(
    const QnResourcePtr& resource,
    nxcip::DtsArchiveReader* archiveReader )
:
    m_resource( resource ),
    m_archiveReader( archiveReader ),
    m_streamReader( NULL )
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
}

ThirdPartyArchiveDelegate::~ThirdPartyArchiveDelegate()
{
    if( m_streamReader )
        m_streamReader->releaseRef();
    m_archiveReader->releaseRef();
}

//!Implementation of QnAbstractArchiveDelegate::open
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

//!Implementation of QnAbstractArchiveDelegate::open
void ThirdPartyArchiveDelegate::close()
{
    if( m_streamReader )
    {
        m_streamReader->releaseRef();
        m_streamReader = NULL;
    }
}

//!Implementation of QnAbstractArchiveDelegate::open
qint64 ThirdPartyArchiveDelegate::startTime()
{
    return m_archiveReader->startTime();
}

//!Implementation of QnAbstractArchiveDelegate::open
qint64 ThirdPartyArchiveDelegate::endTime()
{
    return m_archiveReader->endTime();
}

//!Implementation of QnAbstractArchiveDelegate::open
QnAbstractMediaDataPtr ThirdPartyArchiveDelegate::getNextData()
{
    if( !m_streamReader )
        return QnAbstractMediaDataPtr(0);
    return ThirdPartyStreamReader::readStreamReader( m_streamReader );
}

//!Implementation of QnAbstractArchiveDelegate::open
qint64 ThirdPartyArchiveDelegate::seek( qint64 time, bool findIFrame )
{
    nxcip::UsecUTCTimestamp selectedPosition = 0;
    if( m_archiveReader->seek( time, findIFrame, &selectedPosition ) != nxcip::NX_NO_ERROR )
        return nxcip::INVALID_TIMESTAMP_VALUE;
    return selectedPosition;
}

//!Implementation of QnAbstractArchiveDelegate::open
QnResourceVideoLayout* ThirdPartyArchiveDelegate::getVideoLayout()
{
    //TODO/IMPL
    return NULL;
}

//!Implementation of QnAbstractArchiveDelegate::open
QnResourceAudioLayout* ThirdPartyArchiveDelegate::getAudioLayout()
{
    //TODO/IMPL
    return NULL;
}

//!Implementation of QnAbstractArchiveDelegate::open
void ThirdPartyArchiveDelegate::onReverseMode( qint64 displayTime, bool value )
{
    m_archiveReader->setReverseMode( value, displayTime );
}

//!Implementation of QnAbstractArchiveDelegate::open
void ThirdPartyArchiveDelegate::setSingleshotMode( bool /*value*/ )
{
    //TODO/IMPL
}

//!Implementation of QnAbstractArchiveDelegate::open
bool ThirdPartyArchiveDelegate::setQuality( MediaQuality quality, bool fastSwitch )
{
    return m_archiveReader->setQuality(
        quality == MEDIA_Quality_High || quality == MEDIA_Quality_ForceHigh
            ? nxcip::msqHigh
            : (quality == MEDIA_Quality_Low ? nxcip::msqLow : nxcip::msqDefault),
        fastSwitch );
}

void ThirdPartyArchiveDelegate::setRange( qint64 startTime, qint64 /*endTime*/, qint64 frameStep )
{
    nxcip::UsecUTCTimestamp selectedPosition = 0;
    m_archiveReader->seek( startTime, true, &selectedPosition );
    m_archiveReader->setSkipFrames( frameStep );
    //TODO: #ak when disable setSkipFrames ???
}

void ThirdPartyArchiveDelegate::setSendMotion( bool value )
{
    m_archiveReader->setMotionDataEnabled( value );
}
