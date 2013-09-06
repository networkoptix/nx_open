/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#include "third_party_archive_delegate.h"


ThirdPartyArchiveDelegate::ThirdPartyArchiveDelegate(
    const QnResourcePtr& resource,
    nxcip::DtsArchiveReader* archiveReader )
:
    m_resource( resource ),
    m_archiveReader( archiveReader ),
    m_streamReader( NULL )
{
    unsigned int caps = m_archiveReader->getCapabilities();
    if( caps & nxcip::DtsArchiveReader::reverseModeCapability )
        m_flags |= QnAbstractArchiveDelegate::Flag_CanProcessNegativeSpeed;
    if( caps & nxcip::DtsArchiveReader::motionDataCapability )
        m_flags |= QnAbstractArchiveDelegate::Flag_CanSendMotion;

    m_flags |= QnAbstractArchiveDelegate::Flag_CanOfflineRange;
    m_flags |= QnAbstractArchiveDelegate::Flag_CanSeekImmediatly;
    m_flags |= QnAbstractArchiveDelegate::Flag_CanOfflineLayout;

    //if( caps & nxcip::DtsArchiveReader::searchByMotionMaskCapability )
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
    nxcip::MediaDataPacket* packet = NULL;
    if( m_streamReader->getNextData( &packet ) != nxcip::NX_NO_ERROR )
        return QnAbstractMediaDataPtr();    //error reading data
    if( packet == NULL )
        return QnAbstractMediaDataPtr();    //end of data

    QnAbstractMediaDataPtr mediaPacket;
    switch( packet->type() )
    {
        case nxcip::dptVideo:
        {
            nxcip::VideoDataPacket* srcVideoPacket = static_cast<nxcip::VideoDataPacket*>(packet->queryInterface(nxcip::IID_VideoDataPacket));
            if( !srcVideoPacket )
                return QnAbstractMediaDataPtr();  //looks like bug in plugin implementation

            //mediaPacket = new QnCompressedVideoData();
            //static_cast<QnCompressedVideoData*>(mediaPacket.data())->pts = packet->timestamp();
            break;
        }

        case nxcip::dptAudio:
            //mediaPacket = new QnCompressedAudioData();
            //TODO/IMPL
            break;

        default:
            Q_ASSERT( false );
            break;
    }

    //TODO/IMPL

    return mediaPacket;
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
void ThirdPartyArchiveDelegate::setSingleshotMode( bool value )
{
    //TODO/IMPL
}

//!Implementation of QnAbstractArchiveDelegate::open
bool ThirdPartyArchiveDelegate::setQuality( MediaQuality quality, bool fastSwitch )
{
    //TODO/IMPL
    return false;
}
