/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_stream_reader.h"

#ifdef ENABLE_THIRD_PARTY

#include <algorithm>

#include <QtCore/QTextStream>

#include "utils/network/http/httptypes.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "utils/common/log.h"

#include "core/datapacket/third_party_audio_data_packet.h"
#include "core/datapacket/third_party_video_data_packet.h"

#include "plugins/plugin_tools.h"
#include "plugins/resource/onvif/dataprovider/onvif_mjpeg.h"

#include "motion_data_picture.h"


ThirdPartyStreamReader::ThirdPartyStreamReader(
    QnResourcePtr res,
    nxcip::BaseCameraManager* camManager )
:
    CLServerPushStreamReader( res ),
    m_camManager( camManager ),
    m_liveStreamReader( NULL ),
    m_mediaEncoder2Ref( NULL ),
    m_cameraCapabilities( 0 )
{
    m_thirdPartyRes = getResource().dynamicCast<QnThirdPartyResource>();
    Q_ASSERT( m_thirdPartyRes );

    m_audioLayout.reset( new QnResourceCustomAudioLayout() );

    m_camManager.getCameraCapabilities( &m_cameraCapabilities );
}

ThirdPartyStreamReader::~ThirdPartyStreamReader()
{
    stop();
    if( m_liveStreamReader )
    {
        m_liveStreamReader->releaseRef();
        m_liveStreamReader = NULL;
    }

    if( m_mediaEncoder2Ref )
        m_mediaEncoder2Ref->releaseRef();
}

void ThirdPartyStreamReader::onGotVideoFrame( const QnCompressedVideoDataPtr& videoData )
{
    base_type::onGotVideoFrame( videoData );
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
    if( m_thirdPartyRes->getMotionType() != Qn::MT_HardwareGrid )
        return base_type::updateSoftwareMotion();

    if( m_thirdPartyRes->getVideoLayout()->channelCount() == 0 )
        return;

    nxcip::BaseCameraManager2* camManager2 = static_cast<nxcip::BaseCameraManager2*>(m_camManager.getRef()->queryInterface( nxcip::IID_BaseCameraManager2 ));
    if( !camManager2 )
        return;

    MotionDataPicture* motionMask = new MotionDataPicture( nxcip::PIX_FMT_GRAY8 );
    const QnMotionRegion& region = m_thirdPartyRes->getMotionRegion(0);
    //converting region
    for( int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens )
    {
        foreach( const QRect& rect, region.getRectsBySens(sens) )
        {
            //std::cout<<"Motion mask: sens "<<sens<<", rect ("<<rect.left()<<", "<<rect.top()<<", "<<rect.width()<<", "<<rect.height()<<"\n";

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

    camManager2->releaseRef();
}

CameraDiagnostics::Result ThirdPartyStreamReader::openStream()
{
    if( isStreamOpened() )
        return CameraDiagnostics::NoErrorResult();

    const Qn::ConnectionRole role = getRole();

    const int encoderIndex = role == Qn::CR_LiveVideo
        ? QnThirdPartyResource::PRIMARY_ENCODER_INDEX
        : QnThirdPartyResource::SECONDARY_ENCODER_INDEX;

    nxcip::CameraMediaEncoder* intf = NULL;
    int result = m_camManager.getEncoder( encoderIndex, &intf );
    if( result != nxcip::NX_NO_ERROR )
    {
        QUrl requestedUrl;
        requestedUrl.setHost( m_thirdPartyRes->getHostAddress() );
        requestedUrl.setPort( m_thirdPartyRes->httpPort() );
        requestedUrl.setScheme( QLatin1String("http") );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }
    nxcip_qt::CameraMediaEncoder cameraEncoder( intf );

    m_camManager.setCredentials( m_thirdPartyRes->getAuth().user(), m_thirdPartyRes->getAuth().password() );

    if( m_camManager.setAudioEnabled( m_thirdPartyRes->isAudioEnabled() ) != nxcip::NX_NO_ERROR )
        return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("audio"));

    const nxcip::Resolution& resolution = m_thirdPartyRes->getSelectedResolutionForEncoder( encoderIndex );
    if( resolution.width*resolution.height > 0 )
        if( cameraEncoder.setResolution( resolution ) != nxcip::NX_NO_ERROR )
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("resolution"));
    float selectedFps = 0;
    if( cameraEncoder.setFps( getFps(), &selectedFps ) != nxcip::NX_NO_ERROR )
        return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("fps"));

    int selectedBitrateKbps = 0;
    if( cameraEncoder.setBitrate(
            m_thirdPartyRes->suggestBitrateKbps(getQuality(), QSize(resolution.width, resolution.height), getFps()),
            &selectedBitrateKbps ) != nxcip::NX_NO_ERROR )
    {
        return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("bitrate"));
    }

    m_mediaEncoder2Ref = static_cast<nxcip::CameraMediaEncoder2*>(intf->queryInterface( nxcip::IID_CameraMediaEncoder2 ));
    if( m_mediaEncoder2Ref && (m_liveStreamReader = m_mediaEncoder2Ref->getLiveStreamReader()) )
    {
        if( m_thirdPartyRes->isAudioEnabled() )
        {
            nxcip::AudioFormat audioFormat;
            if( m_mediaEncoder2Ref->getAudioFormat( &audioFormat ) == nxcip::NX_NO_ERROR )
                initializeAudioContext( audioFormat );
        }

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

        NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(mediaUrlStr).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);

        //checking url type and creating corresponding data provider

        QUrl mediaUrl( mediaUrlStr );
        if( mediaUrl.scheme().toLower() == lit("rtsp") )
        {
            QnMulticodecRtpReader* rtspStreamReader = new QnMulticodecRtpReader( m_resource );
            rtspStreamReader->setRequest( mediaUrlStr );
            rtspStreamReader->setRole(role);
            m_builtinStreamReader.reset( rtspStreamReader );
        }
        else if( mediaUrl.scheme().toLower() == lit("http") )
        {
            m_builtinStreamReader.reset( new MJPEGStreamReader(
                m_resource,
                mediaUrl.path() + (!mediaUrl.query().isEmpty() ? lit("?") + mediaUrl.query() : QString()) ) );
        }
        return m_builtinStreamReader->openStream();
    }
}

void ThirdPartyStreamReader::closeStream()
{
    if( m_liveStreamReader )
    {
        m_liveStreamReader->releaseRef();
        m_liveStreamReader = NULL;
    }
    else if( m_builtinStreamReader.get() )
    {
        m_builtinStreamReader->closeStream();
    }
}

bool ThirdPartyStreamReader::isStreamOpened() const
{
    return m_liveStreamReader || (m_builtinStreamReader.get() && m_builtinStreamReader->isStreamOpened());
}

int ThirdPartyStreamReader::getLastResponseCode() const
{
    return m_liveStreamReader
        ? nx_http::StatusCode::ok
        : (m_builtinStreamReader.get() ? m_builtinStreamReader->getLastResponseCode() : nx_http::StatusCode::ok);
}

//bool ThirdPartyStreamReader::needMetaData() const
//{
//    return false;
//}

QnMetaDataV1Ptr ThirdPartyStreamReader::getCameraMetadata()
{
    //TODO/IMPL

    QnMetaDataV1Ptr rez = m_lastMetadata != 0 ? m_lastMetadata : QnMetaDataV1Ptr(new QnMetaDataV1());
    m_lastMetadata.clear();
    return rez;
}

void ThirdPartyStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    if( m_liveStreamReader )
    {
        m_liveStreamReader->interrupt();
    }
    else if( m_builtinStreamReader )
    {
        QnStoppable* stoppable = dynamic_cast<QnStoppable*>(m_builtinStreamReader.get());
        //TODO #ak preferred way to remove dynamic_cast from here and inherit QnAbstractMediaStreamProvider from QnStoppable. 
            //But, this will require virtual inheritance since CLServerPushStreamReader (base of MJPEGStreamReader) indirectly inherits QnStoppable.
        if( stoppable )
            stoppable->pleaseStop();
    }
}

void ThirdPartyStreamReader::beforeRun()
{
    //we can be sure that getNextData will not be called while we are here
    CLServerPushStreamReader::beforeRun();
}

void ThirdPartyStreamReader::afterRun()
{
    //we can be sure that getNextData will not be called while we are here
    CLServerPushStreamReader::afterRun();
    closeStream();
}

Qn::ConnectionRole ThirdPartyStreamReader::roleForMotionEstimation()
{
    const Qn::ConnectionRole softMotionRole = CLServerPushStreamReader::roleForMotionEstimation();
    if( softMotionRole != Qn::CR_LiveVideo)  //primary stream
        return softMotionRole;

    //checking stream resolution
    if( m_videoResolution.width()*m_videoResolution.height() > MAX_PRIMARY_RES_FOR_SOFT_MOTION )
        return Qn::CR_SecondaryLiveVideo;

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

    if( !(m_cameraCapabilities & nxcip::BaseCameraManager::hardwareMotionCapability) && needMetaData() )
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    static const int MAX_TRIES_TO_READ_MEDIA_PACKET = 10;
    for( int i = 0; i < MAX_TRIES_TO_READ_MEDIA_PACKET; ++i )
    {
        if( m_liveStreamReader )
        {
            if( m_savedMediaPacket )
            {
                rez = std::move(m_savedMediaPacket);
                m_savedMediaPacket.clear(); //calling clear since QSharePointer move operator implementation leaves some questions...
            }
            else
            {
                int errorCode = 0;
                rez = readStreamReader( m_liveStreamReader, &errorCode );
                if( rez )
                {
                    rez->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                    QnCompressedVideoData* videoData = dynamic_cast<QnCompressedVideoData*>(rez.data());
                    if( videoData && videoData->motion )
                    {
                        m_savedMediaPacket = rez;
                        rez = std::move(videoData->motion);
                        videoData->motion.clear();
                    }
                    else if( rez->dataType == QnAbstractMediaData::AUDIO )
                    {
                        if( !m_audioContext )
                        {
                            nxcip::AudioFormat audioFormat;
                            if( m_mediaEncoder2Ref->getAudioFormat( &audioFormat ) == nxcip::NX_NO_ERROR )
                                initializeAudioContext( audioFormat );
                        }
                        static_cast<QnCompressedAudioData*>(rez.data())->context = m_audioContext;
                    }
                }
                else
                {
                    if( errorCode == nxcip::NX_NOT_AUTHORIZED )
                        m_thirdPartyRes->setStatus( Qn::Unauthorized );
                }
            }
        }
        else if( m_builtinStreamReader.get() )
        {
            rez = m_builtinStreamReader->getNextData();
        }

        if( rez )
        {
            if( rez->dataType == QnAbstractMediaData::VIDEO )
            {
                //QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
                //parseMotionInfo(videoData);
                break;
            }
            else if (rez->dataType == QnAbstractMediaData::AUDIO)
            {
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

QnConstResourceAudioLayoutPtr ThirdPartyStreamReader::getDPAudioLayout() const
{
    return m_liveStreamReader
        ? m_audioLayout.staticCast<const QnResourceAudioLayout>()    //TODO/IMPL
        : (m_builtinStreamReader.get() ? m_builtinStreamReader->getAudioLayout() : QnConstResourceAudioLayoutPtr());
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
        case nxcip::CODEC_ID_PCM_S16LE:
            return CODEC_ID_PCM_S16LE;
        case nxcip::CODEC_ID_PCM_MULAW:
            return CODEC_ID_PCM_MULAW;
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

QnAbstractMediaDataPtr ThirdPartyStreamReader::readStreamReader( nxcip::StreamReader* streamReader, int* outErrorCode )
{
    nxcip::MediaDataPacket* packet = NULL;
    const int errorCode = streamReader->getNextData( &packet );
    if( outErrorCode )
        *outErrorCode = errorCode;
    if( errorCode != nxcip::NX_NO_ERROR || !packet)
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

            QnThirdPartyCompressedVideoData* videoPacket = new QnThirdPartyCompressedVideoData( srcVideoPacket );
            packetAp.release();
            videoPacket->pts = packet->timestamp();

            nxcip::Picture* srcMotionData = srcVideoPacket->getMotionData();
            if( srcMotionData )
            {
                static const int DEFAULT_MOTION_DURATION = 1000*1000;

                if( srcMotionData->pixelFormat() == nxcip::PIX_FMT_MONOBLACK )
                {
                    //adding motion data
                    QnMetaDataV1Ptr motion( new QnMetaDataV1() );
                    motion->assign( *srcMotionData, srcVideoPacket->timestamp(), DEFAULT_MOTION_DURATION );
                    motion->timestamp = srcVideoPacket->timestamp();
                    motion->channelNumber = packet->channelNumber();
                    motion->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                    videoPacket->motion = motion;
                }
                srcMotionData->releaseRef();
            }

            mediaPacket = QnAbstractMediaDataPtr(videoPacket);
            srcVideoPacket->releaseRef();
            break;
        }

        case nxcip::dptAudio:
            mediaPacket = QnAbstractMediaDataPtr( new QnThirdPartyCompressedAudioData( packetAp.release() ) );
            break;

        case nxcip::dptEmpty:
            mediaPacket = QnAbstractMediaDataPtr( new QnEmptyMediaData() );
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
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_AVKey;
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

    return mediaPacket;
}

void ThirdPartyStreamReader::initializeAudioContext( const nxcip::AudioFormat& audioFormat )
{
    const CodecID ffmpegCodecId = toFFmpegCodecID(audioFormat.compressionType);
    m_audioContext = QnMediaContextPtr( new QnMediaContext(ffmpegCodecId) );

    //filling mediaPacket->context
    m_audioContext->ctx()->codec_id = ffmpegCodecId;
    m_audioContext->ctx()->codec_type = AVMEDIA_TYPE_AUDIO;

    m_audioContext->ctx()->sample_rate = audioFormat.sampleRate;
    m_audioContext->ctx()->bit_rate = audioFormat.bitrate;

    m_audioContext->ctx()->channels = audioFormat.channels;
    //setByteOrder(QnAudioFormat::LittleEndian);
    switch( audioFormat.sampleFmt )
    {
        case nxcip::AudioFormat::stU8:
            m_audioContext->ctx()->sample_fmt = AV_SAMPLE_FMT_U8;
            break;
        case nxcip::AudioFormat::stS16:
            m_audioContext->ctx()->sample_fmt = AV_SAMPLE_FMT_S16;
            break;
        case nxcip::AudioFormat::stS32:
            m_audioContext->ctx()->sample_fmt = AV_SAMPLE_FMT_S32;
            break;
        case nxcip::AudioFormat::stFLT:
            m_audioContext->ctx()->sample_fmt = AV_SAMPLE_FMT_FLT;
            break;
        default:
            assert( false );
    }

    //if (c->extradata_size > 0)
    //{
    //    extraData.resize(c->extradata_size);
    //    memcpy(&extraData[0], c->extradata, c->extradata_size);
    //}
    m_audioContext->ctx()->channel_layout = audioFormat.channelLayout;
    m_audioContext->ctx()->block_align = audioFormat.blockAlign;
    m_audioContext->ctx()->bits_per_coded_sample = audioFormat.bitsPerCodedSample;

    m_audioLayout->addAudioTrack( QnResourceAudioLayout::AudioTrack(m_audioContext, QString()) );
}

QnConstResourceVideoLayoutPtr ThirdPartyStreamReader::getVideoLayout() const
{
    return m_builtinStreamReader ? m_builtinStreamReader->getVideoLayout() : QnConstResourceVideoLayoutPtr();
}

#endif // ENABLE_THIRD_PARTY
