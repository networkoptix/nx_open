// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_consumer.h"

#include <analytics/common/object_metadata.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/reflect/json.h>
#include <nx/reflect/json/serializer.h>
#include <nx/utils/log/log.h>

#include "webrtc_session.h"
#include "webrtc_streamer.h"

namespace nx::webrtc {

Consumer::Consumer(Session* session):
    m_mediaQueue(512),
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

    m_needStop = true;
    m_pollable.pleaseStopSync();
}

void Consumer::startProvidersIfNeeded()
{
    NX_CRITICAL(m_session);
    if (!m_streamer)
        return; //< It is not started yet.
    // Should be called in muxing thread!
    runInMuxingThread(
        [this]()
        {
            for (auto& provider: m_session->allProviders())
            {
                if (!provider->isStarted())
                    provider->play(provider->getPosition(), false);
            }
        });
}

void Consumer::seek(nx::Uuid deviceId, int64_t timestampUs, StatusHandler handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler, deviceId, timestampUs]()
        {
            bool useAudioLastGop = //< TODO is it needed?
                m_session->muxer()->method() == nx::vms::api::WebRtcMethod::mse;
            auto doSeek =
                [this, useAudioLastGop]
                (int64_t timestampUs, auto provider, StatusHandler handler)
                {
                    auto status = provider && provider->play(timestampUs, useAudioLastGop)
                        ? StreamStatus::success
                        : StreamStatus::nodata;

                    m_pollable.dispatch(
                        [deviceId = provider->resource()->getId(), status, handler]()
                        {
                            handler(deviceId, status);
                        });
                };

            if (!deviceId.isNull())
            {
                auto provider = m_session->provider(deviceId);
                doSeek(timestampUs, provider, handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doSeek(timestampUs, provider, handler);
            }
        });
}

void Consumer::changeQuality(
    nx::Uuid deviceId,
    nx::vms::api::StreamIndex stream,
    StatusHandler handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler, deviceId, stream]()
        {
            auto doChangeQuality =
                [this](auto provider, auto stream, auto handler)
                {
                    auto status = provider && provider->changeQuality(stream)
                        ? StreamStatus::success
                        : StreamStatus::nodata;
                    m_pollable.dispatch(
                        [deviceId = provider->resource()->getId(), status, handler]()
                        {
                            handler(deviceId, status);
                        });
                };

            if (!deviceId.isNull())
            {
                auto provider = m_session->provider(deviceId);
                doChangeQuality(provider, stream, handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doChangeQuality(provider, stream, handler);
            }
        });
}

void Consumer::pause(nx::Uuid deviceId, StatusHandler handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler), deviceId]()
        {
            auto doPause =
                [](auto provider, auto handler)
                {
                    auto status = provider && provider->pause()
                        ? StreamStatus::success
                        : StreamStatus::nodata;
                    handler(provider->resource()->getId(), status);
                };

            if (!deviceId.isNull())
            {
                auto provider = m_session->provider(deviceId);
                doPause(provider, handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doPause(provider, handler);
            }
        });
}

void Consumer::resume(nx::Uuid deviceId, StatusHandler handler)
{
    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler = std::move(handler), deviceId]()
        {
            auto doResume =
                [](auto provider, auto handler)
                {
                    auto status = provider && provider->resume()
                        ? StreamStatus::success
                        : StreamStatus::nodata;
                    handler(provider->resource()->getId(), status);
                };

            if (!deviceId.isNull())
            {
                auto provider = m_session->provider(deviceId);
                doResume(provider, handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doResume(provider, handler);
            }
        });
}

void Consumer::nextFrame(nx::Uuid deviceId, StatusHandler handler)
{
    NX_CRITICAL(m_session);

    // Should be called in muxing thread!
    runInMuxingThread(
        [this, handler, deviceId]()
        {
            if (m_session->muxer()->method() == nx::vms::api::WebRtcMethod::mse)
            {
                // Pause/Resume/NextFrame not supported for MSE.
                handler(deviceId, StreamStatus::nodata);
                return;
            }
            auto provider = m_session->provider(deviceId);
            auto status = provider && provider->nextFrame()
                ? StreamStatus::success
                : StreamStatus::nodata;
            handler(deviceId, status);
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
    // Should be posted to avoid deadlock due to recursion.
    if (success)
        m_pollable.post([this]()
        {
            m_sendInProgress = false;
            processNextData();
        });
    else
        m_streamer->ice()->stopUnsafe();
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

void Consumer::processNextData()
{
    // This code should be called only from Streamer's AIO thread.

    while (!m_needStop)
    {
        auto media = m_mediaQueue.popData(false /*blocked*/);
        if (!media)
            break;

        NX_VERBOSE(this, "Process data: %1", media);

        // Send metadata via datachannel.
        const auto metadata = std::dynamic_pointer_cast<const QnCompressedMetadata>(media);
        if (metadata && metadata->metadataType == MetadataType::ObjectDetection && m_enableMetadata)
        {
            sendMetadata(metadata);
            continue;
        }

        const auto deviceId = media->deviceId;
        auto rtpEncoder = m_session->muxer()->setDataPacket(deviceId, media);
        if (!NX_ASSERT(rtpEncoder))
            return;

        nx::utils::ByteArray buffer;
        std::deque<nx::Buffer> mediaBuffers;
        while (!m_needStop && m_session->muxer()->getNextPacket(rtpEncoder, buffer))
        {
            mediaBuffers.emplace_back(buffer.data(), buffer.size());
            buffer.clear();
        }

        // In case with changed codec, encoder will report with EOF.
        // Maybe need to make additional check for extradata: for example, some high h.264 profiles
        // can be unsupported by browser.
        if (m_session->muxer()->isEof(deviceId, rtpEncoder))
        {
            m_pollable.dispatch(
                [this, deviceId]()
                {
                    m_streamer->handleStreamStatus(deviceId, StreamStatus::reconnect);
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

        if (!mediaBuffers.empty())
        {
            // If the ice send buffer is empty, we send the data and start collecting the second chunk.
            // If the ice send buffer is busy, we stop collecting the data and wait for the send with
            // the ready chunk, it will be sent later in the callback onDataSent, so sending is not
            // wait for chunk collection.

            if (m_session->muxer()->method() == nx::vms::api::WebRtcMethod::srtp)
                m_streamer->ice()->writeBatch(mediaBuffers);
            else
                m_streamer->ice()->writeDataChannelBatch(mediaBuffers);
            m_sendInProgress = true;
            break;
        }
    }
}

void Consumer::runInMuxingThread(std::function<void()>&& func)
{
    m_pollable.dispatch(
        [func = std::move(func)]()
        {
            func();
        });
}

void Consumer::putData(const QnConstAbstractMediaDataPtr& data)
{
    m_pollable.dispatch([this, data]()
        {
            m_mediaQueue.putData(data);
            if (!m_sendInProgress)
                processNextData();
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
