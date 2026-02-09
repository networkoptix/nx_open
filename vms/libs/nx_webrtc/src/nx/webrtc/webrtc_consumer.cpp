// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_consumer.h"

#include <analytics/common/object_metadata.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/reflect/json.h>
#include <nx/reflect/json/serializer.h>
#include <nx/utils/log/log.h>

#include "webrtc_session.h"
#include "webrtc_streamer.h"

namespace nx::webrtc {

Consumer::Consumer(Session* session):
    m_session(session)
{}

Consumer::~Consumer()
{
    std::promise<void> stoppedPromise;
    m_pollable.dispatch(
        [this, &stoppedPromise]()
        {
            stopUnsafe();
            stoppedPromise.set_value();
        });
    stoppedPromise.get_future().wait();
}

bool Consumer::startStream(Streamer* streamer)
{
    m_streamer = streamer;
    m_pollable.bindToAioThread(streamer->ice()->getAioThread());
    return true;
}

void Consumer::stopUnsafe()
{
    NX_ASSERT(m_pollable.isInSelfAioThread());

    if (m_streamer)
        m_streamer->stop();
    m_dataConsumedHandler = nullptr;

    m_needStop = true;
    m_pollable.pleaseStopSync();
}

void Consumer::seek(int64_t timestampUs, StatusHandler&& handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler), timestampUs]()
        {
            bool useAudioLastGop = //< TODO is it needed?
                m_session->muxer()->method() == nx::vms::api::WebRtcMethod::mse;
            auto status = m_session->provider()->play(timestampUs, useAudioLastGop)
                ? StreamStatus::success
                : StreamStatus::nodata;

            m_pollable.dispatch(
                [status, handler = std::move(handler)]()
                {
                    handler(status);
                });
            });
}

void Consumer::changeQuality(nx::vms::api::StreamIndex stream, StatusHandler&& handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler), stream]()
        {
            auto status = m_session->provider()->changeQuality(stream)
                ? StreamStatus::success
                : StreamStatus::nodata;
            m_pollable.dispatch(
                [status, handler = std::move(handler)]()
                {
                    handler(status);
                });
        });
}

void Consumer::pause(StatusHandler&& handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler)]()
        {
            auto status = m_session->provider()->pause()
                ? StreamStatus::success
                : StreamStatus::nodata;
            handler(status);
        });
}

void Consumer::resume(StatusHandler&& handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler)]()
        {
            auto status = m_session->provider()->resume()
                ? StreamStatus::success
                : StreamStatus::nodata;
            handler(status);
        });
}

void Consumer::nextFrame(StatusHandler&& handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler)]()
        {
            if (m_session->muxer()->method() == nx::vms::api::WebRtcMethod::mse)
            {
                // Pause/Resume/NextFrame not supported for MSE.
                handler(StreamStatus::nodata);
                return;
            }
            auto status = m_session->provider()->nextFrame()
                ? StreamStatus::success
                : StreamStatus::nodata;
            handler(status);
        });
}

bool Consumer::handleSrtp(std::vector<uint8_t> buffer)
{
    // Should be called in muxing thread!
    runInMuxingThread(
        [this, buffer = std::move(buffer)]() mutable
        {
            using namespace nx::rtp;
            if (buffer.size() < kRtcpHeaderSize)
            {
                NX_DEBUG(this, "Got invalid RTP packet with size %1", buffer.size());
                return;
            }
            auto rtpHeader = (RtpHeader*)(buffer.data());
            if (rtpHeader->isRtcp())
            {
                handleRtcp(buffer.data(), buffer.size());
            }
            else
            {
                NX_VERBOSE(this, "Got unexpected RTP packet from peer");
                // TODO Handle incoming RTP packet.
            }
        });

    return true;
}

void Consumer::handleRtcp(uint8_t* data, int size)
{
    // Should decrypt packet first, due to 'ssrc' field is in encrypted part of packet.
    auto encryptor = m_session->muxer()->getEncryptor();
    if (encryptor)
        encryptor->decryptPacket(data, &size);

    // Several RTCP packets can be glued into one lower level packet.
    while (size >= nx::rtp::kRtcpHeaderSize)
    {
        uint16_t length;
        memcpy(&length, data + 2, sizeof(length));
        length = ntohs(length);
        // length of the RTCP packet in 32-bit words minus one,
        // including the header and any padding.
        int packetLength = std::min((length + 1) * 4, size);
        uint32_t ssrc = nx::rtp::getRtcpSsrc(data, size);
        auto encoder = m_session->muxer()->encoderBySsrc(ssrc);
        if (encoder)
        {
            encoder->setRtcpPacket(data, packetLength);
            nx::utils::ByteArray buffer;
            while (encoder->getNextNackPacket(buffer))
            {
                m_streamer->ice()->writePacket(buffer.data(), buffer.size(), /*foreground*/ false);
                buffer.clear();
            }
        }
        else
        {
            // Some RTCP feedbacks should be demuxed by payloadType.
        }
        size -= packetLength;
        data += packetLength;
    }
}

void Consumer::onDataSent(bool success)
{
    if (!success)
    {
        m_streamer->ice()->stopUnsafe();
        return;
    }

    if (success && !m_mediaBuffers.empty())
    {
        m_pollable.post(  //< Should be posted to avoid deadlock due to recursion.
            [this]() { sendMediaBuffer(); });

        onDataConsumed();
    }
}

void Consumer::onDataConsumed()
{
    m_pollable.post(  //< Should be posted to avoid deadlock due to recursion.
        [this]()
        {
            if (m_dataConsumedHandler)
                m_dataConsumedHandler();
        });
}

void Consumer::sendMetadata(const QnConstCompressedMetadataPtr& metadata)
{
    const QByteArray ref = QByteArray::fromRawData(metadata->data(), metadata->dataSize());
    const auto packet = QnUbjson::deserialized<nx::common::metadata::ObjectMetadataPacket>(ref);
    nx::reflect::json::SerializationContext ctx;
    ctx.composer.startObject();
    ctx.composer.writeAttributeName("metadata");
    nx::reflect::json::serialize(&ctx, packet);
    ctx.composer.endObject(1);
    m_streamer->ice()->writeDataChannelString(ctx.composer.take());
}

void Consumer::putMediaDataToSendBuffers(
    nx::Uuid deviceId,
    const QnConstAbstractMediaDataPtr& media)
{
    // This code should be called only from Streamer's AIO thread.
    NX_VERBOSE(this, "Process data: %1", media);

    if (!media) //< EOF.
    {
        NX_DEBUG(this, "Switch to live, since no data in archive");
        m_session->provider()->play(DATETIME_NOW, /*useAudioLastGop*/ false);
        onDataConsumed();
        return;
    }

    // Send metadata via datachannel.
    const auto metadata = std::dynamic_pointer_cast<const QnCompressedMetadata>(media);
    if (metadata && metadata->metadataType == MetadataType::ObjectDetection && m_enableMetadata)
    {
        sendMetadata(metadata);
        onDataConsumed();
        return;
    }

    auto rtpEncoder = m_session->muxer()->setDataPacket(deviceId, media);

    nx::utils::ByteArray buffer;
    while (!m_needStop && m_session->muxer()->getNextPacket(rtpEncoder, buffer))
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_mediaBuffers.emplace_back(buffer.data(), buffer.size());
        buffer.clear();
    }

    // In case with changed codec, encoder will report with EOF.
    // Maybe need to make additional check for extradata: for example, some high h.264 profiles
    // can be unsupported by browser.
    if (m_session->muxer()->isEof(rtpEncoder))
    {
        m_pollable.dispatch(
            [this]()
            {
                m_streamer->handleStreamStatus(StreamStatus::reconnect);
            });
        return;
    }

    if (media->dataType == QnAbstractMediaData::VIDEO ||
        media->dataType == QnAbstractMediaData::AUDIO)
    {
        auto timestamps = m_session->muxer()->getLastTimestamps();
        if (m_lastReportedTimestampUs == AV_NOPTS_VALUE ||
            std::abs(timestamps.ntpTimestamp - m_lastReportedTimestampUs) > m_sendTimestampInterval.count() * 1000)
        {

            m_lastReportedTimestampUs = timestamps.ntpTimestamp;
            m_streamer->sendTimestamp(timestamps.ntpTimestamp, timestamps.rtpTimestamp);
        }
    }

    if (m_mediaBuffers.empty())
    {
        onDataConsumed();
        return;
    }

    if (!m_needStop && m_streamer->ice()->bufferEmpty())
    {
        // If the ice send buffer is empty, we send the data and start collecting the second chunk.
        // If the ice send buffer is busy, we stop collecting the data and wait for the send with
        // the ready chunk, it will be sent later in the callback onDataSent, so sending is not
        // wait for chunk collection.
        sendMediaBuffer();
        onDataConsumed();
    }
}

void Consumer::sendMediaBuffer()
{
    if (m_session->muxer()->method() == nx::vms::api::WebRtcMethod::srtp)
        m_streamer->ice()->writeBatch(m_mediaBuffers);
    else
        m_streamer->ice()->writeDataChannelBatch(m_mediaBuffers);
    m_mediaBuffers.clear();
}

void Consumer::runInMuxingThread(std::function<void()>&& func)
{
    m_pollable.dispatch(
        [func = std::move(func)]()
        {
            func();
        });
}

void Consumer::putData(
    nx::Uuid deviceId,
    const QnConstAbstractMediaDataPtr& data,
    const AbstractCameraDataProvider::OnDataReady& handler)
{
    m_dataConsumedHandler = handler;
    m_pollable.dispatch(
        [this, deviceId, data]()
        {
            if (m_needStop)
                return;

            putMediaDataToSendBuffers(deviceId, data);
        });
}

void Consumer::setSendTimestampInterval(std::chrono::milliseconds interval)
{
    m_sendTimestampInterval = interval;
}

void Consumer::setEnableMetadata(bool enableMetadata)
{
    m_enableMetadata = enableMetadata;
}

} // namespace nx::webrtc
