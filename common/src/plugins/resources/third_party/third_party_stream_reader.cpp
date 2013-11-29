/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_stream_reader.h"

#include <algorithm>

#include <QtCore/QTextStream>

#include "plugins/plugin_tools.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "utils/network/http/httptypes.h"
#include "motion_data_picture.h"


ThirdPartyStreamReader::ThirdPartyStreamReader(
    QnResourcePtr res,
    nxcip::BaseCameraManager* camManager )
:
    CLServerPushStreamReader( res ),
    m_rtpStreamParser( res ),
    m_camManager( camManager ),
    m_liveStreamReader( NULL )
{
    m_thirdPartyRes = getResource().dynamicCast<QnThirdPartyResource>();
    Q_ASSERT( m_thirdPartyRes );
}

ThirdPartyStreamReader::~ThirdPartyStreamReader()
{
    stop();
    if( m_liveStreamReader )
    {
        m_liveStreamReader->releaseRef();
        m_liveStreamReader = NULL;
    }
}

static const nxcip::Resolution DEFAULT_SECOND_STREAM_RESOLUTION = nxcip::Resolution(480, 316);

void ThirdPartyStreamReader::onGotVideoFrame( QnCompressedVideoDataPtr videoData )
{
    //TODO/IMPL
    parent_type::onGotVideoFrame( videoData );
}

static int sensitivityToMask[10] = 
{
    255, //  0
    26,
    22,
    18,
    16,
    14,
    12,
    11,
    10,
    9, // 9
};

void ThirdPartyStreamReader::updateSoftwareMotion()
{
    nxcip::BaseCameraManager2* camManager2 = static_cast<nxcip::BaseCameraManager2*>(m_camManager.getRef()->queryInterface( nxcip::IID_BaseCameraManager2 ));
    if( !camManager2 )
        return;

    if( m_thirdPartyRes->getMotionType() == Qn::MT_SoftwareGrid && getRole() == roleForMotionEstimation() )
    {
        if( m_thirdPartyRes->getVideoLayout()->channelCount() > 0 )
        {
            MotionDataPicture* motionMask = new MotionDataPicture( nxcip::PIX_FMT_GRAY8 );
            const QnMotionRegion& region = m_thirdPartyRes->getMotionRegion(0);
            //converting region
            for( int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens )
            {
                foreach( const QRect& rect, region.getRectsBySens(sens) )
                {
                    for( int y = rect.top(); y <= rect.bottom(); ++y )
                        for( int x = rect.left(); x <= rect.right(); ++x )
                        {
                            assert( y < motionMask->width() && x < motionMask->height() );
                            motionMask->setPixel( y, x, sensitivityToMask[sens] );
                            //m_motionMask[x * MD_HEIGHT + y] = sensitivityToMask[sens];
                            //m_motionSensMask[x * MD_HEIGHT + y] = sens;
                        }
                }
            }
            camManager2->setMotionMask( motionMask );
            motionMask->releaseRef();
        }
    }

    camManager2->releaseRef();
}

CameraDiagnostics::Result ThirdPartyStreamReader::openStream()
{
    if( isStreamOpened() )
        return CameraDiagnostics::NoErrorResult();

    QnResource::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);
    const int encoderNumber = role == QnResource::Role_LiveVideo ? 0 : 1;

    nxcip::CameraMediaEncoder* intf = NULL;
    int result = m_camManager.getEncoder( encoderNumber, &intf );
    if( result != nxcip::NX_NO_ERROR )
    {
        QUrl requestedUrl;
        requestedUrl.setHost( m_thirdPartyRes->getHostAddress() );
        requestedUrl.setPort( m_thirdPartyRes->httpPort() );
        requestedUrl.setScheme( QLatin1String("http") );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }
    nxcip_qt::CameraMediaEncoder cameraEncoder( intf );

    if( m_camManager.setAudioEnabled( m_thirdPartyRes->isAudioEnabled() ) != nxcip::NX_NO_ERROR )
        return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("audio"));

    const nxcip::Resolution& resolution = (role == QnResource::Role_LiveVideo) 
        ? getMaxResolution(encoderNumber) 
        : getNearestResolution(encoderNumber, DEFAULT_SECOND_STREAM_RESOLUTION);
    if( resolution.width*resolution.height > 0 )
        if( cameraEncoder.setResolution( resolution ) != nxcip::NX_NO_ERROR )
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("resolution"));
    float selectedFps = 0;
    if( cameraEncoder.setFps( getFps(), &selectedFps ) != nxcip::NX_NO_ERROR )
        return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("fps"));

    nxcip::CameraMediaEncoder2* mediaEncoder2Ref = static_cast<nxcip::CameraMediaEncoder2*>(intf->queryInterface( nxcip::IID_CameraMediaEncoder2 ));
    if( mediaEncoder2Ref && (m_liveStreamReader = mediaEncoder2Ref->getLiveStreamReader()) )
    {
        mediaEncoder2Ref->releaseRef();
        return CameraDiagnostics::NoErrorResult();
    }
    else
    {
        //opening url
        QString mediaUrlStr;
        if( cameraEncoder.getMediaUrl( &mediaUrlStr ) != nxcip::NX_NO_ERROR )
        {
            QUrl requestedUrl;
            requestedUrl.setHost( m_thirdPartyRes->getHostAddress() );
            requestedUrl.setPort( m_thirdPartyRes->httpPort() );
            requestedUrl.setScheme( QLatin1String("http") );
            return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
        }

        m_rtpStreamParser.setRequest( mediaUrlStr );
        return m_rtpStreamParser.openStream();
    }
}

void ThirdPartyStreamReader::closeStream()
{
    if( m_liveStreamReader )
    {
        m_liveStreamReader->releaseRef();
        m_liveStreamReader = NULL;
    }
    else
    {
        m_rtpStreamParser.closeStream();
    }
}

bool ThirdPartyStreamReader::isStreamOpened() const
{
    return m_liveStreamReader || m_rtpStreamParser.isStreamOpened();
}

int ThirdPartyStreamReader::getLastResponseCode() const
{
    return m_liveStreamReader
        ? nx_http::StatusCode::ok
        : m_rtpStreamParser.getLastResponseCode();
}

QnMetaDataV1Ptr ThirdPartyStreamReader::getCameraMetadata()
{
    //TODO/IMPL

    QnMetaDataV1Ptr rez = m_lastMetadata != 0 ? m_lastMetadata : QnMetaDataV1Ptr(new QnMetaDataV1());
    m_lastMetadata.clear();
    return rez;
}

void ThirdPartyStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    if( m_liveStreamReader )
        m_liveStreamReader->interrupt();
    else
        m_rtpStreamParser.pleaseStop();
}

QnResource::ConnectionRole ThirdPartyStreamReader::roleForMotionEstimation()
{
    const QnResource::ConnectionRole softMotionRole = CLServerPushStreamReader::roleForMotionEstimation();
    if( softMotionRole != QnResource::Role_LiveVideo)  //primary stream
        return softMotionRole;

    //checking stream resolution
    if( m_videoResolution.width()*m_videoResolution.height() > MAX_PRIMARY_RES_FOR_SOFT_MOTION )
        return QnResource::Role_SecondaryLiveVideo;

    return softMotionRole;
}

void ThirdPartyStreamReader::onStreamResolutionChanged( int /*channelNumber*/, const QSize& picSize )
{
    m_videoResolution = picSize;
}

QnAbstractMediaDataPtr ThirdPartyStreamReader::getNextData()
{
    if( !isStreamOpened() )
    {
        openStream();
        if( !isStreamOpened() )
            return QnAbstractMediaDataPtr(0);
    }

    if( needMetaData() )
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    static const int MAX_TRIES_TO_READ_MEDIA_PACKET = 10;
    for( int i = 0; i < MAX_TRIES_TO_READ_MEDIA_PACKET; ++i )
    {
        if( m_liveStreamReader )
        {
            if( m_savedMediaPacket )
            {
                rez = m_savedMediaPacket;
                m_savedMediaPacket = QnAbstractMediaDataPtr();
            }
            else
            {
                rez = readStreamReader( m_liveStreamReader );
                if( rez )
                {
                    rez->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                    QnCompressedVideoDataPtr videoData = rez.dynamicCast<QnCompressedVideoData>();
                    if( videoData && videoData->motion )
                    {
                        rez = videoData->motion;
                        videoData->motion = QnMetaDataV1Ptr();
                        m_savedMediaPacket = videoData;
                    }
                }
            }
        }
        else
        {
            rez = m_rtpStreamParser.getNextData();
        }

        if( rez )
        {
            if( rez->dataType == QnAbstractMediaData::VIDEO )
            {
                QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
                //parseMotionInfo(videoData);
                break;
            }
            else if (rez->dataType == QnAbstractMediaData::AUDIO) {
                break;
            }
            else if (rez->dataType == QnAbstractMediaData::META_V1)
            {
                break;
            }
        }
        else {
            closeStream();
            break;
        }
    }

    return rez;
}

void ThirdPartyStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void ThirdPartyStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

const QnResourceAudioLayout* ThirdPartyStreamReader::getDPAudioLayout() const
{
    return m_liveStreamReader
        ? NULL    //TODO/IMPL
        : m_rtpStreamParser.getAudioLayout();
}

CodecID ThirdPartyStreamReader::toFFmpegCodecID( nxcip::CompressionType compressionType )
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

QnAbstractMediaDataPtr ThirdPartyStreamReader::readStreamReader( nxcip::StreamReader* streamReader )
{
    nxcip::MediaDataPacket* packet = NULL;
    if( streamReader->getNextData( &packet ) != nxcip::NX_NO_ERROR || !packet)
        return QnAbstractMediaDataPtr();    //error reading data

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

            nxcip::Picture* srcMotionData = srcVideoPacket->getMotionData();
            if( srcMotionData )
            {
                static const int DEFAULT_MOTION_DURATION = 35*1000; //~ 30 fps

                if( srcMotionData->pixelFormat() == nxcip::PIX_FMT_MONOBLACK )
                {
                    //adding motion data
                    QnMetaDataV1Ptr motion( new QnMetaDataV1() );
                    motion->assign( *srcMotionData, srcVideoPacket->timestamp(), DEFAULT_MOTION_DURATION );
                    motion->timestamp = srcVideoPacket->timestamp();
                    motion->channelNumber = packet->channelNumber();
                    static_cast<QnCompressedVideoData*>(mediaPacket.data())->motion = motion;
                }
                srcMotionData->releaseRef();
            }

            srcVideoPacket->releaseRef();
            break;
        }

        case nxcip::dptAudio:
            mediaPacket = QnAbstractMediaDataPtr(new QnCompressedAudioData());
            mediaPacket->dataType = QnAbstractMediaData::AUDIO;
            break;

        case nxcip::dptEmpty:
            mediaPacket = QnAbstractMediaDataPtr(new QnEmptyMediaData());
            break;    //end of data

        default:
            Q_ASSERT( false );
            break;
    }

    mediaPacket->compressionType = toFFmpegCodecID( packet->codecType() );
    mediaPacket->opaque = packet->cSeq();
    mediaPacket->channelNumber = packet->channelNumber();
    if( packet->type() != nxcip::dptEmpty )
    {
        mediaPacket->timestamp = packet->timestamp();
    }
    else
    {
        if( packet->flags() & nxcip::MediaDataPacket::fReverseStream )
        {
            mediaPacket->flags |= QnAbstractMediaData::MediaFlags_Reverse;
            mediaPacket->flags |= QnAbstractMediaData::MediaFlags_ReverseReordered;
            mediaPacket->timestamp = 0;
        }
        else
        {
            mediaPacket->timestamp = DATETIME_NOW;
        }
    }
    if( packet->flags() & nxcip::MediaDataPacket::fKeyPacket )
        mediaPacket->flags |= AV_PKT_FLAG_KEY;
    if( packet->flags() & nxcip::MediaDataPacket::fReverseStream )
    {
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_Reverse;
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_ReverseReordered;
    }
    if( packet->flags() & nxcip::MediaDataPacket::fReverseBlockStart )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
    if( packet->flags() & nxcip::MediaDataPacket::fLowQuality )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_LowQuality;
    if( packet->flags() & nxcip::MediaDataPacket::fStreamReset )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_BOF;

    //QnMediaContextPtr context;
    //int opaque;

    //TODO/IMPL get rid of following copy by modifying QnAbstractMediaData class
    if( packet->data() )
        mediaPacket->data.write( (const char*)packet->data(), packet->dataSize() );
    return mediaPacket;
}

static bool resolutionLess( const nxcip::Resolution& left, const nxcip::Resolution& right )
{
    return left.width*left.height < right.width*right.height;
}

nxcip::Resolution ThirdPartyStreamReader::getMaxResolution( int encoderNumber ) const
{
    const QList<nxcip::Resolution>& resolutionList = m_thirdPartyRes->getEncoderResolutionList( encoderNumber );
    QList<nxcip::Resolution>::const_iterator maxResIter = std::max_element( resolutionList.constBegin(), resolutionList.constEnd(), resolutionLess );
    return maxResIter != resolutionList.constEnd() ? *maxResIter : nxcip::Resolution();
}

nxcip::Resolution ThirdPartyStreamReader::getNearestResolution( int encoderNumber, const nxcip::Resolution& desiredResolution ) const
{
    const QList<nxcip::Resolution>& resolutionList = m_thirdPartyRes->getEncoderResolutionList( encoderNumber );
    nxcip::Resolution foundResolution;
    foreach( nxcip::Resolution resolution, resolutionList )
    {
        if( resolution.width*resolution.height <= desiredResolution.width*desiredResolution.height &&
            resolution.width*resolution.height > foundResolution.width*foundResolution.height )
        {
            foundResolution = resolution;
        }
    }
    return foundResolution;
}
