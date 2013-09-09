/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#include "third_party_archive_delegate.h"

#include <plugins/plugin_tools.h>


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
    nxcip::MediaDataPacket* packet = NULL;
    if( m_streamReader->getNextData( &packet ) != nxcip::NX_NO_ERROR )
        return QnAbstractMediaDataPtr();    //error reading data
    if( packet == NULL )
        return QnAbstractMediaDataPtr( new QnEmptyMediaData() );    //end of data

    nxpt::ScopedRef<nxcip::MediaDataPacket> packetAp( packet, false );

    QnAbstractMediaDataPtr mediaPacket;
    switch( packet->type() )
    {
        case nxcip::dptVideo:
        {
            nxcip::VideoDataPacket* srcVideoPacket = static_cast<nxcip::VideoDataPacket*>(packet->queryInterface(nxcip::IID_VideoDataPacket));
            if( !srcVideoPacket )
                return QnAbstractMediaDataPtr();  //looks like bug in plugin implementation

            mediaPacket = QnAbstractMediaDataPtr(new QnCompressedVideoData());
            static_cast<QnCompressedVideoData*>(mediaPacket.data())->pts = packet->timestamp();
            mediaPacket->dataType = QnAbstractMediaData::VIDEO;

            //TODO/IMPL adding motion data
            break;
        }

        case nxcip::dptAudio:
            mediaPacket = QnAbstractMediaDataPtr(new QnCompressedAudioData());
            mediaPacket->dataType = QnAbstractMediaData::AUDIO;
            break;

        default:
            Q_ASSERT( false );
            break;
    }

    mediaPacket->compressionType = toFFmpegCodecID( packet->codecType() );
    mediaPacket->channelNumber = packet->channelNumber();
    if( packet->flags() & nxcip::MediaDataPacket::fKeyPacket )
        mediaPacket->flags |= AV_PKT_FLAG_KEY;
    if( packet->flags() & nxcip::MediaDataPacket::fReverseStream )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_Reverse;
    if( packet->flags() & nxcip::MediaDataPacket::fReverseBlockStart )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
    if( packet->flags() & nxcip::MediaDataPacket::fLowQuality )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_LowQuality;
    if( packet->flags() & nxcip::MediaDataPacket::fStreamReset )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_BOF;
    //QnMediaContextPtr context;
    //int opaque;

    //TODO/IMPL data

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

CodecID ThirdPartyArchiveDelegate::toFFmpegCodecID( nxcip::CompressionType compressionType )
{
    switch( compressionType )
    {
        case nxcip::CODEC_ID_MPEG2VIDEO:
            return CODEC_ID_MPEG2VIDEO;
        case nxcip::CODEC_ID_H263:
            return CODEC_ID_H263;
        case nxcip::CODEC_ID_MJPEG:
            return CODEC_ID_MJPEG;
        case nxcip::CODEC_ID_MPEG4:
            return CODEC_ID_MPEG4;
        case nxcip::CODEC_ID_H264:
            return CODEC_ID_H264;
        case nxcip::CODEC_ID_THEORA:
            return CODEC_ID_THEORA;
        case nxcip::CODEC_ID_PNG:
            return CODEC_ID_PNG;
        case nxcip::CODEC_ID_GIF:
            return CODEC_ID_GIF;
        case nxcip::CODEC_ID_MP2:
            return CODEC_ID_MP2;
        case nxcip::CODEC_ID_MP3:
            return CODEC_ID_MP3;
        case nxcip::CODEC_ID_AAC:
            return CODEC_ID_AAC;
        case nxcip::CODEC_ID_AC3:
            return CODEC_ID_AC3;
        case nxcip::CODEC_ID_DTS:
            return CODEC_ID_DTS;
        case nxcip::CODEC_ID_VORBIS:
            return CODEC_ID_VORBIS;
        default:
            return CODEC_ID_NONE;
    }
}
