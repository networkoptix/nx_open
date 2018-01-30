
#include "third_party_stream_reader.h"

#ifdef ENABLE_THIRD_PARTY

#include <algorithm>
#include <nx/utils/std/cpp14.h>

#include <QtCore/QTextStream>

#include <nx/network/http/http_types.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <plugins/resource/third_party/motion_data_picture.h>
#include <plugins/resource/onvif/dataprovider/onvif_mjpeg.h>
#include <network/multicodec_rtp_reader.h>

#include <utils/common/app_info.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/frame_type_extractor.h>

#include "third_party_audio_data_packet.h"
#include "third_party_video_data_packet.h"

#include <motion/motion_detection.h>
#include <utils/common/synctime.h>

namespace
{
    bool isIFrame( QnAbstractMediaDataPtr packet )
    {
        AVCodecID codecId = packet->compressionType;

        NX_ASSERT( codecId == AV_CODEC_ID_H264, "IFrame detection", "only AV_CODEC_ID_H264 is supported" );

        if( !packet || codecId != AV_CODEC_ID_H264 )
            return false;

        const void * data = packet->data();
        const quint8 * udata = static_cast<const quint8 *>(data);

        FrameTypeExtractor frameTypeEx(codecId);
        auto type = frameTypeEx.getFrameType( udata, packet->dataSize() );

        switch(type)
        {
            case FrameTypeExtractor::I_Frame:
                return true;

            default:
                break;
        }

        return false;
    }

    void refDeleter(nxpl::PluginInterface * ptr)
    {
        if( ptr )
            ptr->releaseRef();
    }
}


ThirdPartyStreamReader::ThirdPartyStreamReader(
    QnResourcePtr res,
    nxcip::BaseCameraManager* camManager )
:
    CLServerPushStreamReader( res ),
    m_camManager( camManager ),
    m_cameraCapabilities( 0 )
{
    m_thirdPartyRes = getResource().dynamicCast<QnThirdPartyResource>();
    NX_ASSERT( m_thirdPartyRes );

    m_audioLayout.reset( new QnResourceCustomAudioLayout() );

    m_camManager.getCameraCapabilities( &m_cameraCapabilities );
}

ThirdPartyStreamReader::~ThirdPartyStreamReader()
{
    stop();
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

    MotionDataPicture* motionMask = new MotionDataPicture( nxcip::AV_PIX_FMT_GRAY8 );
    const QnMotionRegion& region = m_thirdPartyRes->getMotionRegion(0);
    //converting region
    for( int sens = 0; sens < QnMotionRegion::kSensitivityLevelCount; ++sens )
    {
        for( const QRect& rect: region.getRectsBySens(sens) )
        {
            //std::cout<<"Motion mask: sens "<<sens<<", rect ("<<rect.left()<<", "<<rect.top()<<", "<<rect.width()<<", "<<rect.height()<<"\n";

            for( int y = rect.top(); y <= rect.bottom(); ++y )
                for( int x = rect.left(); x <= rect.right(); ++x )
                {
                    NX_ASSERT( y < motionMask->width() && x < motionMask->height() );
                    motionMask->setPixel( y, x, sensitivityToMask[sens] );
                    //m_motionMask[x * Qn::kMotionGridHeight + y] = sensitivityToMask[sens];
                    //m_motionSensMask[x * Qn::kMotionGridHeight + y] = sens;
                }
        }
    }
    camManager2->setMotionMask( motionMask );
    motionMask->releaseRef();

    camManager2->releaseRef();
}

CameraDiagnostics::Result ThirdPartyStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& liveStreamParams)
{
    QnLiveStreamParams params = liveStreamParams;
    if( isStreamOpened() )
        return CameraDiagnostics::NoErrorResult();

    const Qn::ConnectionRole role = getRole();

    const auto encoderIndex = role == Qn::CR_LiveVideo
        ? Qn::StreamIndex::primary
        : Qn::StreamIndex::secondary;

    nxcip::CameraMediaEncoder* intf = NULL;
    int result = m_camManager.getEncoder( (int) encoderIndex, &intf );
    if( result != nxcip::NX_NO_ERROR )
    {
        QUrl requestedUrl;
        requestedUrl.setHost( m_thirdPartyRes->getHostAddress() );
        requestedUrl.setPort( m_thirdPartyRes->httpPort() );
        requestedUrl.setScheme( QLatin1String("http") );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }
    nxcip_qt::CameraMediaEncoder cameraEncoder( intf );

    QAuthenticator auth = m_thirdPartyRes->getAuth();
    m_camManager.setCredentials( auth.user(), auth.password() );

    m_mediaEncoder2.reset();
    m_mediaEncoder2.reset( static_cast<nxcip::CameraMediaEncoder2*>(intf->queryInterface( nxcip::IID_CameraMediaEncoder2 )), refDeleter );

    nxpt::ScopedRef<nxcip::CameraMediaEncoder3> mediaEncoder3(
        (nxcip::CameraMediaEncoder3*)intf->queryInterface( nxcip::IID_CameraMediaEncoder3 ),
        false );

    if (mediaEncoder3.get()) // one-call config
    {
        if( isCameraControlRequired )
        {
            const nxcip::Resolution& resolution = m_thirdPartyRes->getSelectedResolutionForEncoder( encoderIndex );
            // TODO: advanced params control is not implemented for this driver yet
            params.resolution = QSize(resolution.width, resolution.height);
            int bitrateKbps = m_thirdPartyRes->suggestBitrateKbps(params, getRole() );

            nxcip::LiveStreamConfig config;
            memset(&config, 0, sizeof(nxcip::LiveStreamConfig));
            config.width = resolution.width;
            config.height = resolution.height;
            config.framerate = params.fps;
            config.bitrateKbps = bitrateKbps;
            config.quality = params.quality;
            if( (m_cameraCapabilities & nxcip::BaseCameraManager::audioCapability) && m_thirdPartyRes->isAudioEnabled() )
                config.flags |= nxcip::LiveStreamConfig::LIVE_STREAM_FLAG_AUDIO_ENABLED;

            QnMutexLocker lock(&m_streamReaderMutex);
            m_liveStreamReader.reset(); // release an old one first
            lock.unlock();
            nxcip::StreamReader * tmpReader = nullptr;
            int ret = mediaEncoder3->getConfiguredLiveStreamReader(&config, &tmpReader);
            lock.relock();
            m_liveStreamReader.reset( tmpReader, refDeleter );

            if( ret != nxcip::NX_NO_ERROR )
                return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("stream"));
        }
        else
        {
            QnMutexLocker lock(&m_streamReaderMutex);
            m_liveStreamReader.reset();
            lock.unlock();
            auto liveReader = mediaEncoder3->getLiveStreamReader();
            lock.relock();
            m_liveStreamReader.reset( liveReader, refDeleter );
        }
    }
    else // multiple-calls config
    {
        if( isCameraControlRequired )
        {
            if( m_cameraCapabilities & nxcip::BaseCameraManager::audioCapability )
            {
                if( m_camManager.setAudioEnabled( m_thirdPartyRes->isAudioEnabled() ) != nxcip::NX_NO_ERROR )
                    return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("audio"));
            }

            const nxcip::Resolution& resolution = m_thirdPartyRes->getSelectedResolutionForEncoder( encoderIndex );
            if( resolution.width*resolution.height > 0 )
                if( cameraEncoder.setResolution( resolution ) != nxcip::NX_NO_ERROR )
                    return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("resolution"));

            float selectedFps = 0;
            if( cameraEncoder.setFps( params.fps, &selectedFps ) != nxcip::NX_NO_ERROR )
                return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("fps"));

            int selectedBitrateKbps = 0;
            // TODO: advanced params control is not implemented for this driver yet
            params.resolution = QSize(resolution.width, resolution.height);
            if( cameraEncoder.setBitrate(
                m_thirdPartyRes->suggestBitrateKbps(params, getRole()),
                    &selectedBitrateKbps ) != nxcip::NX_NO_ERROR )
            {
                return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("bitrate"));
            }
        }

        if( m_mediaEncoder2 )
        {
            QnMutexLocker lock(&m_streamReaderMutex);
            m_liveStreamReader.reset();
            lock.unlock();
            auto liveReader = m_mediaEncoder2->getLiveStreamReader();
            lock.relock();
            m_liveStreamReader.reset( liveReader, refDeleter );
        }
    }

    if( m_mediaEncoder2 && m_liveStreamReader )
    {
        if( m_thirdPartyRes->isAudioEnabled() )
        {
            nxcip::AudioFormat audioFormat;
            Extras extras;
            if( m_mediaEncoder2->getAudioFormat( &audioFormat ) == nxcip::NX_NO_ERROR )
                initializeAudioContext( audioFormat, extras );
        }

        char mediaUrlBuf[nxcip::MAX_TEXT_LEN];
        auto error = m_mediaEncoder2->getMediaUrl(mediaUrlBuf);

        if (error == nxcip::NX_NO_ERROR)
        {
            QString mediaUrlStr(mediaUrlBuf);
            m_thirdPartyRes->updateSourceUrl(mediaUrlStr, getRole());
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

        m_thirdPartyRes->updateSourceUrl(mediaUrlStr, getRole());
        NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(mediaUrlStr).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);

        //checking url type and creating corresponding data provider

        QUrl mediaUrl( mediaUrlStr );
        if( mediaUrl.scheme().toLower() == lit("rtsp") )
        {
            QnMulticodecRtpReader* rtspStreamReader = new QnMulticodecRtpReader( m_resource );
            rtspStreamReader->setUserAgent(nx::utils::AppInfo::productName());
            rtspStreamReader->setRequest( mediaUrlStr );
            rtspStreamReader->setRole(role);
            m_thirdPartyRes->updateSourceUrl(rtspStreamReader->getCurrentStreamUrl(), getRole());
            QnMutexLocker lock(&m_streamReaderMutex);
            m_builtinStreamReader.reset( rtspStreamReader );
        }
        else if( mediaUrl.scheme().toLower() == lit("http") )
        {
            QnMutexLocker lock(&m_streamReaderMutex);
            m_builtinStreamReader.reset(new MJPEGStreamReader(
                m_resource,
                mediaUrl.path() + (!mediaUrl.query().isEmpty() ? lit("?") + mediaUrl.query() : QString())));
        }
        else
        {
            return CameraDiagnostics::UnknownErrorResult();
        }
        return m_builtinStreamReader->openStream();
    }
}

void ThirdPartyStreamReader::closeStream()
{
    QnMutexLocker lock(&m_streamReaderMutex);

    m_liveStreamReader.reset();

    if( m_builtinStreamReader.get() )
        m_builtinStreamReader->closeStream();
    m_videoTimeHelpers.clear();
    m_audioTimeHelpers.clear();
}

bool ThirdPartyStreamReader::isStreamOpened() const
{
    QnMutexLocker lock(&m_streamReaderMutex);
    return m_liveStreamReader || (m_builtinStreamReader.get() && m_builtinStreamReader->isStreamOpened());
}

int ThirdPartyStreamReader::getLastResponseCode() const
{
    QnMutexLocker lock(&m_streamReaderMutex);
    return m_liveStreamReader
        ? nx::network::http::StatusCode::ok
        : (m_builtinStreamReader.get() ? m_builtinStreamReader->getLastResponseCode() : nx::network::http::StatusCode::ok);
}

//bool ThirdPartyStreamReader::needMetaData() const
//{
//    return false;
//}

QnMetaDataV1Ptr ThirdPartyStreamReader::getCameraMetadata()
{
    //TODO/IMPL

    QnMetaDataV1Ptr rez;
    if( m_lastMetadata )
        rez.swap( m_lastMetadata );
    else
        rez = QnMetaDataV1Ptr(new QnMetaDataV1());
    return rez;
}

void ThirdPartyStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    QnMutexLocker lock(&m_streamReaderMutex);
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
    auto forcedMotionDetection = m_thirdPartyRes->getProperty(QnMediaResource::motionStreamKey());
    auto forcedPrimaryStreamMotionDetection =
        forcedMotionDetection == QnMediaResource::primaryStreamValue();

    if( m_videoResolution.width()*m_videoResolution.height() > MAX_PRIMARY_RES_FOR_SOFT_MOTION
        && !forcedPrimaryStreamMotionDetection)
    {
        return Qn::CR_SecondaryLiveVideo;
    }

    return softMotionRole;
}

void ThirdPartyStreamReader::onStreamResolutionChanged( int /*channelNumber*/, const QSize& picSize )
{
    m_videoResolution = picSize;
}

QnAbstractMediaDataPtr ThirdPartyStreamReader::getNextData()
{
    if( !isStreamOpened() )
        return QnAbstractMediaDataPtr(0);

    if( !(m_cameraCapabilities & nxcip::BaseCameraManager::hardwareMotionCapability) && needMetadata() )
        return getMetadata();

    QnAbstractMediaDataPtr rez;
    static const int MAX_TRIES_TO_READ_MEDIA_PACKET = 10;
    for( int i = 0; i < MAX_TRIES_TO_READ_MEDIA_PACKET; ++i )
    {
        if( m_liveStreamReader )
        {
            if( m_savedMediaPacket )
            {
                rez = std::move(m_savedMediaPacket);
                m_savedMediaPacket.reset(); //calling clear since QSharePointer move operator implementation leaves some questions...
            }
            else
            {
                int errorCode = 0;
                Extras extras;
                rez = readStreamReader( m_liveStreamReader.get(), &errorCode, &extras);
                if( rez )
                {
                    if( m_cameraCapabilities & nxcip::BaseCameraManager::needIFrameDetectionCapability )
                    {
                        if( isIFrame(rez) )
                            rez->flags |= QnAbstractMediaData::MediaFlags_AVKey;
                    }

                    rez->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                    QnCompressedVideoData* videoData = dynamic_cast<QnCompressedVideoData*>(rez.get());
                    if( videoData && !videoData->metadata.isEmpty() )
                    {
                        m_savedMediaPacket = rez;
                        rez = videoData->metadata.first();
                        videoData->metadata.pop_front();
                    }
                    else if( rez->dataType == QnAbstractMediaData::AUDIO )
                    {
                        if( !m_audioContext || (extras.extradataBlob.size() > 0 && m_audioContext->getExtradataSize() == 0))
                        {
                            nxcip::AudioFormat audioFormat;
                            if( m_mediaEncoder2->getAudioFormat( &audioFormat ) == nxcip::NX_NO_ERROR )
                                initializeAudioContext( audioFormat, extras );
                        }
                        static_cast<QnCompressedAudioData*>(rez.get())->context = m_audioContext;
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

    if (m_needCorrectTime && rez)
    {
        if (auto helper = timeHelper(rez))
            rez->timestamp = helper->getTimeUs(rez->timestamp);
    }

    return rez;
}

void ThirdPartyStreamReader::setNeedCorrectTime(bool value)
{
    m_needCorrectTime = value;
}

nx::utils::TimeHelper* ThirdPartyStreamReader::timeHelper(const QnAbstractMediaDataPtr& data)
{
    std::vector<TimeHelperPtr>* helperList = nullptr;
    if (data->dataType == QnAbstractMediaData::VIDEO)
        helperList = &m_videoTimeHelpers;
    else if (data->dataType == QnAbstractMediaData::AUDIO)
        helperList = &m_audioTimeHelpers;

    if (helperList == nullptr || data->channelNumber >= CL_MAX_CHANNELS)
        return nullptr;
    while (helperList->size() <= data->channelNumber)
    {
        helperList->push_back(
            std::make_unique<nx::utils::TimeHelper>(
                m_resource->getUniqueId(),
                []() { return qnSyncTime->currentTimePoint(); }));
    }
    return helperList->at(data->channelNumber).get();
}

QnConstResourceAudioLayoutPtr ThirdPartyStreamReader::getDPAudioLayout() const
{
    // public function can be called from other thread
    QnMutexLocker lock(&m_streamReaderMutex);
    return m_liveStreamReader
        ? m_audioLayout.staticCast<const QnResourceAudioLayout>()    //TODO/IMPL
        : (m_builtinStreamReader.get() ? m_builtinStreamReader->getAudioLayout() : QnConstResourceAudioLayoutPtr());
}

AVCodecID ThirdPartyStreamReader::toFFmpegCodecID( nxcip::CompressionType compressionType )
{
    switch( compressionType )
    {
        case nxcip::AV_CODEC_ID_MPEG2VIDEO:
            return AV_CODEC_ID_MPEG2VIDEO;
        case nxcip::AV_CODEC_ID_H263:
            return AV_CODEC_ID_H263;
        case nxcip::AV_CODEC_ID_MJPEG:
            return AV_CODEC_ID_MJPEG;
        case nxcip::AV_CODEC_ID_MPEG4:
            return AV_CODEC_ID_MPEG4;
        case nxcip::AV_CODEC_ID_H264:
            return AV_CODEC_ID_H264;
        case nxcip::AV_CODEC_ID_THEORA:
            return AV_CODEC_ID_THEORA;
        case nxcip::AV_CODEC_ID_PNG:
            return AV_CODEC_ID_PNG;
        case nxcip::AV_CODEC_ID_GIF:
            return AV_CODEC_ID_GIF;
        case nxcip::AV_CODEC_ID_MP2:
            return AV_CODEC_ID_MP2;
        case nxcip::AV_CODEC_ID_MP3:
            return AV_CODEC_ID_MP3;
        case nxcip::AV_CODEC_ID_AAC:
            return AV_CODEC_ID_AAC;
        case nxcip::AV_CODEC_ID_PCM_S16LE:
            return AV_CODEC_ID_PCM_S16LE;
        case nxcip::AV_CODEC_ID_PCM_MULAW:
            return AV_CODEC_ID_PCM_MULAW;
        case nxcip::AV_CODEC_ID_AC3:
            return AV_CODEC_ID_AC3;
        case nxcip::AV_CODEC_ID_DTS:
            return AV_CODEC_ID_DTS;
        case nxcip::AV_CODEC_ID_VORBIS:
            return AV_CODEC_ID_VORBIS;
        default:
            return AV_CODEC_ID_NONE;
    }
}

QnAbstractMediaDataPtr ThirdPartyStreamReader::readStreamReader(
    nxcip::StreamReader* streamReader,
    int* outErrorCode,
    Extras* outExtras)
{
    nxcip::MediaDataPacket* packet = NULL;
    const int errorCode = streamReader->getNextData( &packet );
    if( outErrorCode )
        *outErrorCode = errorCode;
    if( errorCode != nxcip::NX_NO_ERROR || !packet)
        return QnAbstractMediaDataPtr();    //error reading data

    nxpt::ScopedRef<nxcip::MediaDataPacket> packetAp( packet, false );

    QnAbstractMediaDataPtr mediaPacket;

    nxcip::MediaDataPacket2* mediaDataPacket2 = static_cast<nxcip::MediaDataPacket2*>(
        packet->queryInterface(nxcip::IID_MediaDataPacket2));

    if (mediaDataPacket2)
    {
        auto extradataSize = mediaDataPacket2->extradataSize();
        outExtras->extradataBlob.resize(extradataSize);
        memcpy(outExtras->extradataBlob.data(), mediaDataPacket2->extradata(), mediaDataPacket2->extradataSize());
        mediaDataPacket2->releaseRef();
    }

    switch( packet->type() )
    {
        case nxcip::dptVideo:
        {
            QnThirdPartyCompressedVideoData* videoPacket = nullptr;
            nxcip::VideoDataPacket* srcVideoPacket = static_cast<nxcip::VideoDataPacket*>(packet->queryInterface(nxcip::IID_VideoDataPacket));

            if (srcVideoPacket)
            {
                if( !srcVideoPacket )
                    return QnAbstractMediaDataPtr();  //looks like bug in plugin implementation

                videoPacket = new QnThirdPartyCompressedVideoData( srcVideoPacket );
                packetAp.release();

                // leave PTS unchanged
                //videoPacket->pts = packet->timestamp();

                nxcip::Picture* srcMotionData = srcVideoPacket->getMotionData();
                if( srcMotionData )
                {
                    static const int DEFAULT_MOTION_DURATION = 1000*1000;

                    if( srcMotionData->pixelFormat() == nxcip::AV_PIX_FMT_MONOBLACK )
                    {
                        //adding motion data
                        QnMetaDataV1Ptr motion( new QnMetaDataV1() );
                        const nxcip::Picture& motionPicture = *srcMotionData;

                        if( motionPicture.pixelFormat() == nxcip::AV_PIX_FMT_MONOBLACK )
                        {
                            NX_ASSERT( motionPicture.width() == Qn::kMotionGridHeight && motionPicture.height() == Qn::kMotionGridWidth );
                            NX_ASSERT( motionPicture.xStride(0) * CHAR_BIT == motionPicture.width() );

                            motion->assign( motionPicture.data(), srcVideoPacket->timestamp(), DEFAULT_MOTION_DURATION );
                        }

                        motion->timestamp = srcVideoPacket->timestamp();
                        motion->channelNumber = packet->channelNumber();
                        motion->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                        videoPacket->metadata << motion;
                    }
                    srcMotionData->releaseRef();
                }
                srcVideoPacket->releaseRef();
            }
            else
                videoPacket = new QnThirdPartyCompressedVideoData( packetAp.release() );

            mediaPacket = QnAbstractMediaDataPtr(videoPacket);
            break;
        }

        case nxcip::dptAudio:
            mediaPacket = QnAbstractMediaDataPtr( new QnThirdPartyCompressedAudioData( packetAp.release() ) );
            break;

        case nxcip::dptEmpty:
            mediaPacket = QnAbstractMediaDataPtr( new QnEmptyMediaData() );
            break;    //end of data

        default:
            NX_ASSERT( false );
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
    {
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_AVKey;
    }
    if( packet->flags() & nxcip::MediaDataPacket::fReverseStream )
    {
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_Reverse;
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_ReverseReordered;
    }
    if( packet->flags() & nxcip::MediaDataPacket::fReverseBlockStart )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
    if( packet->flags() & nxcip::MediaDataPacket::fStreamReset )
        mediaPacket->flags |= QnAbstractMediaData::MediaFlags_BOF;

    return mediaPacket;
}

void ThirdPartyStreamReader::initializeAudioContext( const nxcip::AudioFormat& audioFormat, const Extras& extras )
{
    const AVCodecID ffmpegCodecId = toFFmpegCodecID(audioFormat.compressionType);
    const auto context = new QnAvCodecMediaContext(ffmpegCodecId);
    m_audioContext = QnConstMediaContextPtr(context);
    const auto av = context->getAvCodecContext();

    //filling mediaPacket->context
    av->codec_id = ffmpegCodecId;
    av->codec_type = AVMEDIA_TYPE_AUDIO;

    av->sample_rate = audioFormat.sampleRate;
    av->bit_rate = audioFormat.bitrate;

    av->channels = audioFormat.channels;
    //setByteOrder(QnAudioFormat::LittleEndian);
    switch( audioFormat.sampleFmt )
    {
        case nxcip::AudioFormat::stU8:
            av->sample_fmt = AV_SAMPLE_FMT_U8;
            break;
        case nxcip::AudioFormat::stS16:
            av->sample_fmt = AV_SAMPLE_FMT_S16;
            break;
        case nxcip::AudioFormat::stS32:
            av->sample_fmt = AV_SAMPLE_FMT_S32;
            break;
        case nxcip::AudioFormat::stFLT:
            av->sample_fmt = AV_SAMPLE_FMT_FLT;
            break;
        default:
            NX_ASSERT( false );
    }

    //if (c->extradata_size > 0)
    //{
    //    extraData.resize(c->extradata_size);
    //    memcpy(&extraData[0], c->extradata, c->extradata_size);
    //}
    av->channel_layout = audioFormat.channelLayout;
    av->block_align = audioFormat.blockAlign;
    av->bits_per_coded_sample = audioFormat.bitsPerCodedSample;
    av->time_base.num = 1;
    av->time_base.den = audioFormat.sampleRate;

    if (extras.extradataBlob.size() > 0 && context->getExtradataSize() == 0)
    {
        context->setExtradata(
            (const uint8_t*) extras.extradataBlob.data(),
            extras.extradataBlob.size());
    }

    m_audioLayout.reset(new QnResourceCustomAudioLayout());
    m_audioLayout->addAudioTrack( QnResourceAudioLayout::AudioTrack(m_audioContext, QString()));
}

QnConstResourceVideoLayoutPtr ThirdPartyStreamReader::getVideoLayout() const
{
    // public function can be called from other thread
    QnMutexLocker lock(&m_streamReaderMutex);
    return m_builtinStreamReader ? m_builtinStreamReader->getVideoLayout() : QnConstResourceVideoLayoutPtr();
}

#endif // ENABLE_THIRD_PARTY
