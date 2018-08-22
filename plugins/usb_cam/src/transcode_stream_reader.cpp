#include "transcode_stream_reader.h"

#include <stdio.h>
#include "nx/utils/app_info.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"s

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "utils.h"
#include "device/utils.h"
#include "ffmpeg/video_stream_reader.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

namespace {
int constexpr RETRY_LIMIT = 10;
}

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    StreamReaderPrivate(
        encoderIndex,
        codecParams,
        camera),
    m_cameraState(kOff),
    m_retries(0),
    m_initCode(0),
    m_consumer(new ffmpeg::BufferedVideoFrameConsumer(camera->videoStreamReader(), codecParams))
{
    std::cout << "TranscodeStreamReader" << std::endl;
}


TranscodeStreamReader::~TranscodeStreamReader()
{
    m_camera->videoStreamReader()->removeFrameConsumer(m_consumer);
    uninitialize();
    std::cout << "~TranscodeStreamReader" << std::endl;
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

   if (m_retries >= RETRY_LIMIT)
        return nxcip::NX_NO_DATA;

    if (!ensureInitialized())
    {
        ++m_retries;
        return nxcip::NX_OTHER_ERROR;
    }

    ensureConsumerAdded();
    maybeDropFrames();

    int nxError;
    nxcip::DataPacketType mediaType = nxcip::dptEmpty;
    auto packet = nextPacket(&mediaType, &nxError);
    if(m_interrupted)
    {
        m_interrupted = false;
        return nxcip::NX_INTERRUPTED;
    }

    if(nxError != nxcip::NX_NO_ERROR)
        return nxError;

    *lpPacket = toNxPacket(
        packet->packet(),
        m_encoder->codecID(),
        mediaType,
        getNxTimeStamp(packet->pts()) * 1000,
        false).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::interrupt()
{
    StreamReaderPrivate::interrupt();
    m_camera->videoStreamReader()->removeFrameConsumer(m_consumer);
    m_consumer->interrupt();
    m_consumer->flush();
}

void TranscodeStreamReader::setFps(float fps)
{
    if (m_codecParams.fps != fps)
    {
        StreamReaderPrivate::setFps(fps);
        m_consumer->setFps(fps);
        m_cameraState = kModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.width != resolution.width || m_codecParams.height != resolution.height)
    {
        StreamReaderPrivate::setResolution(resolution);
        m_consumer->setResolution(resolution.width, resolution.height);
        m_cameraState = kModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        StreamReaderPrivate::setBitrate(bitrate);
        m_consumer->setBitrate(bitrate);
        m_cameraState = kModified;
    }
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::transcodeNextPacket(int * nxError)
{
    *nxError = nxcip::NX_NO_ERROR;
    std::shared_ptr<ffmpeg::Frame> frame;
    int dropAmount = m_camera->videoStreamReader()->fps() / m_codecParams.fps - 1;
    dropAmount = dropAmount > 0 ? dropAmount : 1;
    for(int i = 0; i < dropAmount; ++i)
    {
        /**
         * Assign frame to nullptr to prevent a dead lock while waiting for Frame::m_frameCount to drop to 0.
         * when frame still references a valid counted frame from the previous loop iteration,
         * the ffmpeg::VideoStreamReader can't deinitialize, as it is waiting for the count to drop to 0.
         */
        frame = nullptr;
        frame = m_consumer->popFront();
        if(m_interrupted)
        {
            m_interrupted = false;
            *nxError = nxcip::NX_INTERRUPTED;
            return nullptr;
        }
        if (!frame)
        {
            *nxError = nxcip::NX_OTHER_ERROR;
            return nullptr;
        }
    }

    addTimeStamp(frame->pts(), frame->timeStamp());

    scaleNextFrame(frame.get(), nxError);
    if(*nxError != nxcip::NX_NO_ERROR)
        return nullptr;

    std::shared_ptr<ffmpeg::Packet> packet = 
        std::make_shared<ffmpeg::Packet>(m_encoder->codecID());
    
    encode(packet.get(), nxError);
    if(*nxError != nxcip::NX_NO_ERROR)
        return nullptr;

    packet->setTimeStamp(getNxTimeStamp(frame->pts()));
    return packet;
}

void TranscodeStreamReader::scaleNextFrame(const ffmpeg::Frame * frame, int * nxError)
{
    int scaleCode = scale(frame->frame(), m_scaledFrame->frame());
    if (scaleCode < 0)
    {
        NX_DEBUG(this) << "scale error:" << ffmpeg::utils::errorToString(scaleCode);
        *nxError = nxcip::NX_OTHER_ERROR;
        return;
    }

    av_frame_copy_props(m_scaledFrame->frame(), frame->frame());
    *nxError = nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::encode(ffmpeg::Packet * outPacket, int * nxError)
{
    bool gotPacket = false;
    while (!gotPacket)
    {
        int returnCode = m_encoder->sendFrame(m_scaledFrame->frame());
        bool needReceive = returnCode == 0 || returnCode == AVERROR(EAGAIN);
        while (needReceive)
        {
            returnCode = m_encoder->receivePacket(outPacket->packet());
            if (returnCode == 0 || returnCode == AVERROR(EAGAIN))
            {
                gotPacket = returnCode == 0;
                break;
            }
            else // something very bad happened
            {
                *nxError = nxcip::NX_OTHER_ERROR;
                return;
            }
        }
        if (!needReceive) // something very bad happened
        {
            *nxError = nxcip::NX_OTHER_ERROR;
            return;
        }
    }

    *nxError = nxcip::NX_NO_ERROR;
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::nextPacket(nxcip::DataPacketType * outMediaType, int * outNxError)
{
    const auto videoFrame = m_consumer->peekFront();
    const auto audioPacket = m_audioConsumer->peekFront();

    *outNxError = nxcip::NX_NO_ERROR;

    if(videoFrame && audioPacket)
    {
        if (audioPacket->timeStamp() <= videoFrame->timeStamp())
        {
            *outMediaType = nxcip::dptAudio;
            return m_audioConsumer->popFront();
        }
        else
        {
            *outMediaType = nxcip::dptVideo;
            return transcodeNextPacket(outNxError);
        }
    }
    else if (!videoFrame && audioPacket)
    {
        *outMediaType = nxcip::dptAudio;
        return m_audioConsumer->popFront();
    }
    else
    {
        *outMediaType = nxcip::dptVideo;
        return transcodeNextPacket(outNxError);
    }
}

void TranscodeStreamReader::addTimeStamp(int64_t ffmpegPts, int64_t nxTimeStamp)
{
    m_timeStamps.addTimeStamp(ffmpegPts, nxTimeStamp);
}

int64_t TranscodeStreamReader::getNxTimeStamp(int64_t ffmpegPts)
{
    int64_t nxTimeStamp;
    if (!m_timeStamps.getNxTimeStamp(ffmpegPts, &nxTimeStamp, true/*eraseEntry*/))
        nxTimeStamp = m_camera->millisSinceEpoch();

    return nxTimeStamp;
}

bool TranscodeStreamReader::ensureInitialized()
{   
    bool needInit = m_cameraState == kOff || m_cameraState == kModified;
    if (m_cameraState == kModified)
        uninitialize();

    if(needInit)
    {
        m_initCode = initialize();
        if(m_initCode < 0)
            m_camera->setLastError(m_initCode);
    }

    return m_cameraState == kInitialized;
}

void TranscodeStreamReader::ensureConsumerAdded()
{
    if (!m_consumerAdded)
    {
        StreamReaderPrivate::ensureConsumerAdded();
        m_camera->videoStreamReader()->addFrameConsumer(m_consumer);
    }
}

int TranscodeStreamReader::initialize()
{
    int result = openVideoEncoder();
    if (result < 0)
    {
        NX_DEBUG(this) << m_camera->videoStreamReader()->url() + ":" 
            << "encoder open error:" << ffmpeg::utils::errorToString(result);
        return result;
    }

    result = initializeScaledFrame(m_encoder.get());
    if(result < 0)
    {
        NX_DEBUG(this) << m_camera->videoStreamReader()->url() + ":" 
            << "scaled frame init error:" << ffmpeg::utils::errorToString(result);
        return result;
    }

    m_cameraState = kInitialized;
    return 0;
}

void TranscodeStreamReader::uninitialize()
{
    m_scaledFrame.reset(nullptr);
    if(m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);
    m_cameraState = kOff;
}

int TranscodeStreamReader::openVideoEncoder()
{
    int result = 0;
    std::unique_ptr<ffmpeg::Codec> encoder;
    auto ffmpegCodecList = utils::ffmpegCodecPriorityList();
    for (const auto & codecID : ffmpegCodecList)
    {
        encoder = std::make_unique<ffmpeg::Codec>();
        result = encoder->initializeEncoder(codecID);
        if (result >= 0)
            break;
    }
    if (result < 0)
        return result;

    setEncoderOptions(encoder.get());

    result = encoder->open();
    if (result < 0)
        return result;

    m_encoder = std::move(encoder);
    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const ffmpeg::Codec* encoder)
{
    NX_ASSERT(encoder);

    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);
    
    AVPixelFormat encoderFormat = 
        ffmpeg::utils::unDeprecatePixelFormat(encoder->codec()->pix_fmts[0]);

    int result = 
        scaledFrame->getBuffer(encoderFormat, m_codecParams.width, m_codecParams.height, 32);
    
    if (result < 0)
        return result;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

void TranscodeStreamReader::setEncoderOptions(ffmpeg::Codec* encoder)
{
    encoder->setFps(m_codecParams.fps);
    encoder->setResolution(m_codecParams.width, m_codecParams.height);
    encoder->setBitrate(m_codecParams.bitrate);
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codecID()));
    
    AVCodecContext* encoderContext = encoder->codecContext();
    /* don't use global header. the rpi cam doesn't stream properly with global. */
    encoderContext->flags = AV_CODEC_FLAG2_LOCAL_HEADER;

    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void TranscodeStreamReader::maybeDropFrames()
{
    if (m_consumer->size() >= m_camera->videoStreamReader()->fps())
        m_consumer->flush();

    if(m_audioConsumer->size() >= 50)
        m_audioConsumer->flush();
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame)
{
    AVPixelFormat pixelFormat = frame->format != -1 
        ? (AVPixelFormat)frame->format 
        : m_camera->videoStreamReader()->decoderPixelFormat();
        
    struct SwsContext * scaleContext = sws_getCachedContext(
        nullptr,
        frame->width,
        frame->height,
        ffmpeg::utils::unDeprecatePixelFormat(pixelFormat),
        m_codecParams.width,
        m_codecParams.height,
        ffmpeg::utils::unDeprecatePixelFormat(m_encoder->codec()->pix_fmts[0]),
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (!scaleContext)
        return AVERROR(ENOMEM);

    int scaleCode = sws_scale(
        scaleContext,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        outFrame->data,
        outFrame->linesize);

    sws_freeContext(scaleContext);

    return scaleCode;
}

} // namespace usb_cam
} // namespace nx
