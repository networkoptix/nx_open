#include "player_data_consumer.h"

#include <algorithm>

#include <core/resource/media_resource.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/debug_utils.h>
#include <nx/utils/log/log.h>

#include "seamless_video_decoder.h"
#include "seamless_audio_decoder.h"
#include "abstract_audio_decoder.h"
#include "frame_metadata.h"
#include "audio_output.h"

namespace nx {
namespace media {

namespace {

// Max queue length for compressed data (about 3 second).
static const int kMaxMediaQueueLen = 90;

// Max queue length for decoded video which is awaiting to be rendered.
static const int kMaxDecodedVideoQueueSize = 2;

/**
 * Player will emit EOF if and only if it gets several empty packets in a row
 * Sometime single empty packet can be received in case of Multi server archive server1->server2 switching
 * or network issue/reconnect.
 */
static const int kEmptyPacketThreshold = 3;

QSize qMax(const QSize& size1, const QSize& size2)
{
    return size1.height() > size2.height() ? size1 : size2;
}

} // namespace

PlayerDataConsumer::PlayerDataConsumer(
    const std::unique_ptr<QnArchiveStreamReader>& archiveReader)
    :
    QnAbstractDataConsumer(kMaxMediaQueueLen),
    m_awaitingJumpCounter(0),
    m_sequence(0),
    m_lastFrameTimeUs(AV_NOPTS_VALUE),
    m_lastDisplayedTimeUs(AV_NOPTS_VALUE),
    m_emptyPacketCounter(0),
    m_audioEnabled(true)
{
    Qn::directConnect(archiveReader.get(), &QnArchiveStreamReader::beforeJump,
        this, &PlayerDataConsumer::onBeforeJump);
    Qn::directConnect(archiveReader.get(), &QnArchiveStreamReader::jumpOccured,
        this, &PlayerDataConsumer::onJumpOccurred);
    Qn::directConnect(archiveReader.get(), &QnArchiveStreamReader::jumpCanceled,
        this, &PlayerDataConsumer::onJumpCanceled);
}

PlayerDataConsumer::~PlayerDataConsumer()
{
    stop();
    directDisconnectAll();
}

void PlayerDataConsumer::pleaseStop()
{
    base_type::pleaseStop();
    QnMutexLocker lock(&m_decoderMutex);
    for (auto& videoDecoder: m_videoDecoders)
        videoDecoder->pleaseStop();
    if (m_audioDecoder)
        m_audioDecoder->pleaseStop();
    m_queueWaitCond.wakeAll();
}

bool PlayerDataConsumer::canAcceptData() const
{
    {
        QnMutexLocker lock(&m_decoderMutex);
        if (m_audioOutput && !m_audioOutput->canAcceptData())
            return false;
    }

    return base_type::canAcceptData();
}

int PlayerDataConsumer::getBufferingMask() const
{
    // TODO: Not implemented yet. Reserved for future use for panoramic cameras.
    return 1;
}

void PlayerDataConsumer::putData(const QnAbstractDataPacketPtr& data)
{
    base_type::putData(data);
}

qint64 PlayerDataConsumer::queueVideoDurationUsec() const
{
    qint64 minTime = std::numeric_limits<qint64>::max();
    qint64 maxTime = 0;
    auto unsafeQueue = m_dataQueue.lock();
    for (int i = 0; i < unsafeQueue.size(); ++i)
    {
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(
            unsafeQueue.at(i));
        if (video)
        {
            minTime = std::min(minTime, video->timestamp);
            maxTime = std::max(maxTime, video->timestamp);
        }
    }
    return std::max(0ll, maxTime - minTime);
}

ConstAudioOutputPtr PlayerDataConsumer::audioOutput() const
{
    QnMutexLocker lock(&m_decoderMutex);
    return m_audioOutput;
}

bool PlayerDataConsumer::processData(const QnAbstractDataPacketPtr& data)
{
    {
        if (!m_audioEnabled && m_audioOutput)
        {
            QnMutexLocker lock(&m_decoderMutex);
            m_audioOutput.reset();
        }
    }

    auto emptyFrame = std::dynamic_pointer_cast<QnEmptyMediaData>(data);
    if (emptyFrame)
        return processEmptyFrame(emptyFrame);
    m_emptyPacketCounter = 0;

    auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (videoFrame)
        return processVideoFrame(videoFrame);

    auto audioFrame = std::dynamic_pointer_cast<QnCompressedAudioData>(data);
    if (audioFrame && m_audioEnabled)
        return processAudioFrame(audioFrame);

    return true; //< Just ignore unknown frame type.
}

bool PlayerDataConsumer::processEmptyFrame(const QnEmptyMediaDataPtr& data)
{
    if (!data->flags.testFlag(QnAbstractMediaData::MediaFlags_GotFromRemotePeer))
        return true; //< Ignore locally generated packets. It occurs when TCP connection is closed.

    if (m_awaitingJumpCounter > 0)
        return true; //< ignore EOF due to we are going set new position

    ++m_emptyPacketCounter;
    if (m_emptyPacketCounter > kEmptyPacketThreshold)
    {
        QVideoFramePtr eofPacket(new QVideoFrame());
        FrameMetadata metadata = FrameMetadata(data);
        metadata.serialize(eofPacket);
        enqueueVideoFrame(eofPacket);
    }
    return true;
}

QnCompressedVideoDataPtr PlayerDataConsumer::queueVideoFrame(
    const QnCompressedVideoDataPtr& videoFrame)
{
    if (!m_audioOutput)
        return videoFrame; //< Pre-decoding queue is not required.

    m_predecodeQueue.push_back(videoFrame);

    QnCompressedVideoDataPtr result = m_predecodeQueue.front();
    if (result->timestamp < m_audioOutput->playbackPositionUsec())
    {
        // Frame time is earlier than the audio buffer, no need to delay it anyway.
        m_predecodeQueue.pop_front();
        return result;
    }

    if (m_audioOutput->isBuffering())
    {
        // Frame time is inside the audio buffer, delay the frame while audio is buffered.
        return QnCompressedVideoDataPtr();
    }
    m_predecodeQueue.pop_front();
    return result;
}

bool PlayerDataConsumer::processVideoFrame(const QnCompressedVideoDataPtr& videoFrame)
{
    if (!checkSequence(videoFrame->opaque))
    {
        //NX_LOG(lit("PlayerDataConsumer::processVideoFrame(): Ignoring old frame"), cl_logDEBUG2);
        return true; //< No error. Just ignore the old frame.
    }

    quint32 videoChannel = videoFrame->channelNumber;
    auto archiveReader = dynamic_cast<const QnArchiveStreamReader*>(videoFrame->dataProvider);
    if (archiveReader)
    {
        auto resource = archiveReader->getResource();
        if (resource)
        {
            if (auto camera = resource.dynamicCast<QnMediaResource>())
            {
                auto videoLayout = camera->getVideoLayout();
                if (videoLayout)
                    m_awaitingFramesMask.setChannelCount(videoLayout->channelCount());
            }
        }
    }

    {
        QnMutexLocker lock(&m_decoderMutex);
        while (m_videoDecoders.size() <= videoChannel)
        {
            auto videoDecoder = new SeamlessVideoDecoder();
            videoDecoder->setAllowOverlay(m_allowOverlay);
            videoDecoder->setVideoGeometryAccessor(m_videoGeometryAccessor);
            m_videoDecoders.push_back(SeamlessVideoDecoderPtr(videoDecoder));
        }
    }
    SeamlessVideoDecoder* videoDecoder = m_videoDecoders[videoChannel].get();

    QnCompressedVideoDataPtr data = queueVideoFrame(videoFrame);
    if (!data)
    {
        //NX_LOG(lit("PlayerDataConsumer::processVideoFrame(): queueVideoFrame() -> null"), cl_logDEBUG2);
        return true; //< The frame is processed.
    }

    QVideoFramePtr decodedFrame;
    if (!videoDecoder->decode(data, &decodedFrame))
    {
        NX_LOG(lit("Cannot decode the video frame. The frame is skipped."), cl_logWARNING);
        // False result means we want to repeat this frame later, thus, returning true.
    }
    else
    {
        if (decodedFrame)
        {
            //NX_LOG(lit("PlayerDataConsumer::processVideoFrame(): enqueueVideoFrame()"), cl_logDEBUG2);
            enqueueVideoFrame(std::move(decodedFrame));
        }
        else
        {
            //NX_LOG(lit("PlayerDataConsumer::processVideoFrame(): decodedFrame is null"), cl_logDEBUG2);
        }
    }

    return true;
}

bool PlayerDataConsumer::checkSequence(int sequence)
{
    m_sequence = std::max(m_sequence, sequence);
    if (sequence && m_sequence && sequence != m_sequence)
    {
        //NX_LOG(lit("PlayerDataConsumer::checkSequence(%1): expected %2").arg(sequence).arg(m_sequence), cl_logDEBUG2);
        return false;
    }
    return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QVideoFramePtr decodedFrame)
{
    NX_ASSERT(decodedFrame);
    QnMutexLocker lock(&m_queueMutex);
    while (m_decodedVideo.size() >= kMaxDecodedVideoQueueSize && !needToStop())
        m_queueWaitCond.wait(&m_queueMutex);
    if (needToStop())
        return;
    FrameMetadata metadata = FrameMetadata::deserialize(decodedFrame);
    if (!checkSequence(metadata.sequence))
        return; //< ignore old frame
    m_decodedVideo.push_back(std::move(decodedFrame));
    lock.unlock();
    emit gotVideoFrame();
}

void PlayerDataConsumer::clearUnprocessedData()
{
    base_type::clearUnprocessedData();
    QnMutexLocker lock(&m_queueMutex);
    m_decodedVideo.clear();
    m_queueWaitCond.wakeAll();
}

QVideoFramePtr PlayerDataConsumer::dequeueVideoFrame()
{
    QnMutexLocker lock(&m_queueMutex);
    if (m_decodedVideo.empty())
        return QVideoFramePtr(); //< no data
    QVideoFramePtr result = std::move(m_decodedVideo.front());
    m_decodedVideo.pop_front();
    lock.unlock();

    FrameMetadata metadata = FrameMetadata::deserialize(result);

    {
        QnMutexLocker lock(&m_jumpMutex);
        if (!checkSequence(metadata.sequence) || m_awaitingJumpCounter > 0)
        {
            // Frame became deprecated because a new jump was queued. Mark it as noDelay
            metadata.displayHint = DisplayHint::noDelay;
        }
        else if (metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_Ignore))
        {
            metadata.displayHint = DisplayHint::noDelay; //< coarse time frame
        }
        else if (!m_awaitingFramesMask.isEmpty())
        {
            m_awaitingFramesMask.removeChannel(metadata.videoChannel);
            if (m_awaitingFramesMask.isEmpty())
                metadata.displayHint = DisplayHint::firstRegular;
            else
                metadata.displayHint = DisplayHint::noDelay;
        }
    }

    metadata.serialize(result);

    m_queueWaitCond.wakeAll();

    if (result)
        m_lastFrameTimeUs = result->startTime() * 1000;
    return result;
}

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data)
{
    {
        QnMutexLocker lock(&m_decoderMutex);
        if (!m_audioDecoder)
            m_audioDecoder.reset(new SeamlessAudioDecoder());
    }

    AudioFramePtr decodedFrame;
    if (!m_audioDecoder->decode(data, &decodedFrame))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode audio frame. Frame is skipped.";
        return true; // False result means we want to repeat this frame later.
    }

    if (!decodedFrame || !decodedFrame->context)
        return true; //< decoder is buffering. true means input frame processed

    if (!m_audioOutput)
    {
        QnMutexLocker lock(&m_decoderMutex);
        m_audioOutput.reset(new AudioOutput(kInitialBufferMs * 1000, kMaxLiveBufferMs * 1000));
    }
    m_audioOutput->write(decodedFrame);
    return true;
}

void PlayerDataConsumer::onBeforeJump(qint64 timeUsec)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    QnMutexLocker lock(&m_jumpMutex);
    ++m_awaitingJumpCounter;
    m_emptyPacketCounter = 0; //< ignore EOF due to we are going to set new position

    // The purpose of this variable is prevent doing delay between frames while they are displayed.
    // We supposed to decode/display them at maximum speed unless the last jump command is
    // processed.
    m_lastDisplayedTimeUs = m_lastFrameTimeUs = timeUsec; //< force position to the new place
}

void PlayerDataConsumer::onJumpCanceled(qint64 /*timeUsec*/)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    // Previous jump command has not been executed due to the new jump command received.
    QnMutexLocker lock(&m_jumpMutex);
    --m_awaitingJumpCounter;
    NX_ASSERT(m_awaitingJumpCounter >= 0);
}

void PlayerDataConsumer::onJumpOccurred(qint64 /*timeUsec*/, int sequence)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    {
        QnMutexLocker lock(&m_jumpMutex);
        --m_awaitingJumpCounter;
        if (m_awaitingJumpCounter == 0)
        {
            // This function is called from dataProvider thread. PlayerConsumer may still process the
            // previous frame. So, leave noDelay state a bit later, when the next BOF frame will be
            // received.
            m_sequence = sequence;
            m_awaitingFramesMask.setMask();
        }
    }

    clearUnprocessedData(); //< Clear input (undecoded) data queue.
    if (m_awaitingJumpCounter == 0)
        emit jumpOccurred(m_sequence);
    else
        emit hurryUp();
}

void PlayerDataConsumer::endOfRun()
{
    QnMutexLocker lock(&m_decoderMutex);
    m_videoDecoders.clear();
    m_audioDecoder.reset();
}

QSize PlayerDataConsumer::currentResolution() const
{
    QSize result;
    for (const auto& decoder: m_videoDecoders)
        result = qMax(result, decoder->currentResolution());

    return result;
}

AVCodecID PlayerDataConsumer::currentCodec() const
{
    for (const auto& videoDecoder: m_videoDecoders)
    {
        auto result = videoDecoder->currentCodec();
        if (result != AV_CODEC_ID_NONE)
            return result;
    }
    return AV_CODEC_ID_NONE;
}

std::vector<AbstractVideoDecoder*> PlayerDataConsumer::currentVideoDecoders() const
{
    std::vector<AbstractVideoDecoder*> result;

    for (const auto& videoDecoder: m_videoDecoders)
    {
        if (const auto decoder = videoDecoder->currentDecoder())
            result.push_back(decoder);
    }

    return result;
}

void PlayerDataConsumer::setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor)
{
    NX_ASSERT(videoGeometryAccessor);
    m_videoGeometryAccessor = videoGeometryAccessor;
}

qint64 PlayerDataConsumer::getCurrentTime() const
{
    return m_lastFrameTimeUs;
}

qint64 PlayerDataConsumer::getDisplayedTime() const
{
    return m_lastDisplayedTimeUs;
}

void PlayerDataConsumer::setDisplayedTimeUs(qint64 value)
{
    m_lastDisplayedTimeUs = value;
}

qint64 PlayerDataConsumer::getNextTime() const
{
    return AV_NOPTS_VALUE;
}

qint64 PlayerDataConsumer::getExternalTime() const
{
    return m_lastDisplayedTimeUs;
}

void PlayerDataConsumer::setAudioEnabled(bool value)
{
    m_audioEnabled = value;
}

void PlayerDataConsumer::setAllowOverlay(bool value)
{
    m_allowOverlay = value;
}

} // namespace media
} // namespace nx
