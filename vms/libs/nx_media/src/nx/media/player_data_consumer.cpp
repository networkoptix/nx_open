// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "player_data_consumer.h"

#include <algorithm>

#include <core/resource/media_resource.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/fusion/serialization/lexical.h>

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
    const std::unique_ptr<QnArchiveStreamReader>& archiveReader,
    RenderContextSynchronizerPtr renderContextSynchronizer)
    :
    QnAbstractDataConsumer(kMaxMediaQueueLen),
    m_sequence(0),
    m_lastFrameTimeUs(AV_NOPTS_VALUE),
    m_lastDisplayedTimeUs(AV_NOPTS_VALUE),
    m_eofPacketCounter(0),
    m_audioEnabled(true),
    m_needToResetAudio(true),
    m_speed(1),
    m_renderContextSynchronizer(renderContextSynchronizer),
    m_archiveReader(archiveReader.get())
{
    Qn::directConnect(archiveReader.get(), &QnArchiveStreamReader::beforeJump,
        this, &PlayerDataConsumer::onBeforeJump);
    Qn::directConnect(archiveReader.get(), &QnArchiveStreamReader::jumpOccurred,
        this, &PlayerDataConsumer::onJumpOccurred);
}

PlayerDataConsumer::~PlayerDataConsumer()
{
    pleaseStop();
    clearUnprocessedData(); //< We need to clear decoded frames before stopping.
    stop();
    directDisconnectAll();
}

void PlayerDataConsumer::pleaseStop()
{
    base_type::pleaseStop();
    NX_MUTEX_LOCKER lock(&m_decoderMutex);
    for (auto& videoDecoder: m_videoDecoders)
        videoDecoder->pleaseStop();
    m_queueWaitCond.wakeAll();
}

bool PlayerDataConsumer::canAcceptData() const
{
    {
        NX_MUTEX_LOCKER lock(&m_decoderMutex);
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
    NX_MUTEX_LOCKER lock(&m_decoderMutex);
    return m_audioOutput;
}

nx::vms::common::MediaStreamEventPacket PlayerDataConsumer::mediaEvent() const
{
    return m_mediaEvent;
}

void PlayerDataConsumer::updateMediaEvent(const QnAbstractMediaDataPtr& data)
{
    static const auto getMediaEvent =
        [](const QnAbstractMediaDataPtr& data)
        {
            nx::vms::common::MediaStreamEventPacket mediaEvent;

            const auto metadata = std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(data);
            if (!metadata || metadata->metadataType != MetadataType::MediaStreamEvent)
                return mediaEvent;

            const auto buffer = QByteArray::fromRawData(metadata->data(), int(metadata->dataSize()));
            nx::vms::common::deserialize(buffer, &mediaEvent);
            return mediaEvent;
        };

    const auto mediaEvent = getMediaEvent(data);
    if (mediaEvent == m_mediaEvent)
        return;

    m_mediaEvent = mediaEvent;
    emit mediaEventChanged();
}

bool PlayerDataConsumer::processData(const QnAbstractDataPacketPtr& data)
{
    if (m_needToResetAudio)
    {
        m_needToResetAudio = false;
        NX_MUTEX_LOCKER lock(&m_decoderMutex);
        m_audioOutput.reset();
    }

    if (needToStop())
        return true;

    const auto mediaData = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!mediaData)
        return true;

    if (!checkSequence(mediaData->opaque))
    {
        NX_VERBOSE(this, nx::format("PlayerDataConsumer::processData(): Ignoring old frame"));
        return true; //< No error. Just ignore the old frame.
    }

    auto emptyFrame = std::dynamic_pointer_cast<QnEmptyMediaData>(data);
    if (emptyFrame)
        return processEmptyFrame(emptyFrame);
    m_eofPacketCounter = 0;

    updateMediaEvent(mediaData);

    const auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (videoFrame)
        return processVideoFrame(videoFrame);

    const auto audioFrame = std::dynamic_pointer_cast<QnCompressedAudioData>(data);
    if (audioFrame && m_audioEnabled)
        return processAudioFrame(audioFrame);

    auto metadataFrame = std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(data);
    if (metadataFrame)
        emit gotMetadata(metadataFrame);

    return true; //< Just ignore unknown frame type.
}

bool PlayerDataConsumer::processEmptyFrame(const QnEmptyMediaDataPtr& constData)
{
    if (m_archiveReader->isJumpProcessing())
        return true; //< ignore EOF due to we are going set new position

    QnEmptyMediaDataPtr data = constData;

    // Check MediaFlags_BOF flag for compatibility with old media servers 3.2.
    if (constData->flags.testFlag(QnAbstractMediaData::MediaFlags_BOF))
    {
        data.reset(constData->clone());
        data->flags.setFlag(QnAbstractMediaData::MediaFlags_AfterEOF, true);
    }

    if (data->flags.testFlag(QnAbstractMediaData::MediaFlags_AfterEOF))
    {
        // Allow 'kEmptyPacketThreshold' attempts to reconnect before report an issue
        // because RtspClientArchiveDelegate reports EOF packet on each stream read error.
        ++m_eofPacketCounter;
        if (m_eofPacketCounter <= kEmptyPacketThreshold)
            return true;
    }
    else
    {
        // Non-EOF empty packet means a "filler" packet. Is is used by NVR devices with their own
        // archive in case when this archive plays synchronously across several cameras. Sometimes
        // a channel may stop and wait for another, but the connection to devices stays opened.
        m_eofPacketCounter = 0;
    }

    QVideoFramePtr packet(new QVideoFrame());
    FrameMetadata metadata = FrameMetadata(data);
    metadata.serialize(packet);
    enqueueVideoFrame(packet);
    return true;
}

QnCompressedVideoDataPtr PlayerDataConsumer::queueVideoFrame(
    const QnCompressedVideoDataPtr& videoFrame)
{
    NX_MUTEX_LOCKER lock(&m_queueMutex);
    if (videoFrame)
        m_predecodeQueue.push_back(videoFrame);

    if (m_predecodeQueue.empty())
        return QnCompressedVideoDataPtr();

    QnCompressedVideoDataPtr result = m_predecodeQueue.front();
    if (!m_audioOutput || result->timestamp < m_audioOutput->playbackPositionUsec())
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
    QnCompressedVideoDataPtr data = queueVideoFrame(videoFrame);
    if (!data)
        return true;

    quint32 videoChannel = data->channelNumber;
    auto archiveReader = dynamic_cast<const QnArchiveStreamReader*>(data->dataProvider);
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
        NX_MUTEX_LOCKER lock(&m_decoderMutex);
        while (m_videoDecoders.size() <= videoChannel)
        {
            auto videoDecoder = new SeamlessVideoDecoder(m_renderContextSynchronizer);
            videoDecoder->setAllowOverlay(m_allowOverlay);
            videoDecoder->setAllowHardwareAcceleration(m_allowHardwareAcceleration);
            videoDecoder->setVideoGeometryAccessor(m_videoGeometryAccessor);
            m_videoDecoders.push_back(SeamlessVideoDecoderPtr(videoDecoder));
        }
    }
    SeamlessVideoDecoder* videoDecoder = m_videoDecoders[videoChannel].get();

    QVideoFramePtr decodedFrame;
    if (!videoDecoder->decode(data, &decodedFrame))
    {
        NX_WARNING(this, nx::format("Cannot decode the video frame. The frame is skipped."));
        // False result means we want to repeat this frame later, thus, returning true.
    }
    else
    {
        if (decodedFrame)
        {
            //NX_VERBOSE(this, nx::format("PlayerDataConsumer::processVideoFrame(): enqueueVideoFrame()"));
            enqueueVideoFrame(std::move(decodedFrame));
        }
        else
        {
            //NX_VERBOSE(this, nx::format("PlayerDataConsumer::processVideoFrame(): decodedFrame is null"));
        }
    }

    return true;
}

bool PlayerDataConsumer::checkSequence(int sequence)
{
    m_sequence = std::max(m_sequence, sequence);
    if (sequence && m_sequence && sequence != m_sequence)
    {
        //NX_VERBOSE(this, nx::format("PlayerDataConsumer::checkSequence(%1): expected %2").arg(sequence).arg(m_sequence));
        return false;
    }
    return true;
}

void PlayerDataConsumer::enqueueVideoFrame(QVideoFramePtr decodedFrame)
{
    NX_ASSERT(decodedFrame);
    NX_MUTEX_LOCKER lock(&m_queueMutex);
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
    NX_MUTEX_LOCKER lock(&m_queueMutex);
    m_decodedVideo.clear();
    m_predecodeQueue.clear();
    m_needToResetAudio = true;
    m_queueWaitCond.wakeAll();
}

QVideoFramePtr PlayerDataConsumer::dequeueVideoFrame()
{
    NX_MUTEX_LOCKER lock(&m_queueMutex);
    if (m_decodedVideo.empty())
        return QVideoFramePtr(); //< no data
    QVideoFramePtr result = std::move(m_decodedVideo.front());
    m_decodedVideo.pop_front();
    lock.unlock();

    FrameMetadata metadata = FrameMetadata::deserialize(result);

    {
        NX_MUTEX_LOCKER lock(&m_jumpMutex);
        if (!checkSequence(metadata.sequence) || m_archiveReader->isJumpProcessing())
        {
            // Frame became deprecated because a new jump was queued. Mark it as noDelay
            metadata.displayHint = DisplayHint::obsolete;
        }
        else if (metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_Ignore))
        {
            metadata.displayHint = DisplayHint::obsolete; //< Coarse time frame.
        }
        else if (!m_awaitingFramesMask.isEmpty())
        {
            m_awaitingFramesMask.removeChannel(metadata.videoChannel);
            if (m_awaitingFramesMask.isEmpty())
                metadata.displayHint = DisplayHint::firstRegular;
            else
                metadata.displayHint = DisplayHint::obsolete;
        }
    }

    metadata.serialize(result);

    m_queueWaitCond.wakeAll();

    if (result)
        m_lastFrameTimeUs = result->startTime() * 1000;
    return result;
}

void PlayerDataConsumer::setPlaySpeed(double value)
{
    m_speed = value;
}

bool PlayerDataConsumer::processAudioFrame(const QnCompressedAudioDataPtr& data)
{
    {
        NX_MUTEX_LOCKER lock(&m_decoderMutex);
        if (!m_audioDecoder)
            m_audioDecoder.reset(new SeamlessAudioDecoder());
        if (!m_audioOutput)
            m_audioOutput.reset(new AudioOutput(kInitialBufferMs * 1000, kMaxLiveBufferMs * 1000));
    }

    if (!m_audioDecoder->decode(data, m_speed))
    {
        qWarning() << Q_FUNC_INFO << "Can't decode audio frame. Frame is skipped.";
        return true; // False result means we want to repeat this frame later.
    }

    while(true)
    {
        AudioFramePtr decodedFrame = m_audioDecoder->nextFrame();
        if (!decodedFrame || !decodedFrame->context)
            return true; //< decoder is buffering. true means input frame processed

        emit gotAudioFrame();
        m_audioOutput->write(decodedFrame);
    }

    const auto currentPos = m_audioOutput->playbackPositionUsec();

    {
        NX_MUTEX_LOCKER lock(&m_queueMutex);
        while (!m_audioOutput->isBuffering()
            && !m_predecodeQueue.empty()
            && m_predecodeQueue.front()->timestamp < currentPos)
        {
            lock.unlock();
            if (!processVideoFrame(nullptr))
                return false;
            lock.relock();
        }
    }
    return true;
}

void PlayerDataConsumer::onBeforeJump(qint64 timeUsec)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    NX_MUTEX_LOCKER lock(&m_jumpMutex);
    m_eofPacketCounter = 0; //< ignore EOF due to we are going to set new position

    // The purpose of this variable is prevent doing delay between frames while they are displayed.
    // We supposed to decode/display them at maximum speed unless the last jump command is
    // processed.
    m_lastDisplayedTimeUs = m_lastFrameTimeUs = timeUsec; //< force position to the new place
}

void PlayerDataConsumer::onJumpOccurred(qint64 /*timeUsec*/, int sequence)
{
    // This function is called directly from an archiveReader thread. Should be thread safe.
    bool allJumpsProcessed = !m_archiveReader->isJumpProcessing();
    if (allJumpsProcessed)
    {
        NX_MUTEX_LOCKER lock(&m_jumpMutex);
        // This function is called from dataProvider thread. PlayerConsumer may still process the
        // previous frame. So, leave noDelay state a bit later, when the next BOF frame will be
        // received.
        m_sequence = sequence;
        m_awaitingFramesMask.setMask();
    }

    clearUnprocessedData(); //< Clear input (undecoded) data queue.
    if (allJumpsProcessed)
        emit jumpOccurred(m_sequence);
    else
        emit hurryUp();
}

void PlayerDataConsumer::endOfRun()
{
    NX_MUTEX_LOCKER lock(&m_decoderMutex);
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
    NX_MUTEX_LOCKER lock(&m_decoderMutex);
    if (m_audioOutput)
    {
        if (!value)
            m_audioOutput->suspend();
        else if (!m_audioOutput->isBuffering())
            m_audioOutput->resume();
    }
}

bool PlayerDataConsumer::isAudioEnabled() const
{
    return m_audioEnabled;
}

void PlayerDataConsumer::setAllowOverlay(bool value)
{
    m_allowOverlay = value;
}

void PlayerDataConsumer::setAllowHardwareAcceleration(bool value)
{
    if (m_allowHardwareAcceleration == value)
        return;

    NX_MUTEX_LOCKER lock(&m_decoderMutex);
    for (auto& videoDecoder: m_videoDecoders)
        videoDecoder->setAllowHardwareAcceleration(value);
    m_allowHardwareAcceleration = value;
}

} // namespace media
} // namespace nx
