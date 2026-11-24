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

const int kMediaQueueSize = 512;

namespace {

struct CannotDecryptMedia
{
    nx::Uuid deviceId;
    std::chrono::milliseconds timestampMs{};
};
NX_REFLECTION_INSTRUMENT(CannotDecryptMedia, (deviceId)(timestampMs))
NX_REFLECTION_TAG_TYPE(CannotDecryptMedia, jsonSerializeChronoDurationAsNumber)

} // namespace

Consumer::Consumer(Session* session):
    m_mediaQueue(kMediaQueueSize),
    m_pollable(session->aioThread()),
    m_session(session),
    m_sessionId(session->id())
{
}

Consumer::~Consumer()
{
    m_pollable.executeInAioThreadSync([this]() { stopUnsafe(); });
}

bool Consumer::canAcceptData() const
{
    return m_mediaQueue.size() < kMediaQueueSize;
}

void Consumer::clearQueueForDevice(nx::Uuid deviceId)
{
    m_mediaQueue.clearDataForDevice(deviceId);
}

void Consumer::startStream(Streamer* streamer)
{
    // NOTE: m_pollable is not rebound to the ICE thread here. It stays in the session thread for
    // the whole lifetime of the consumer, see Session::aioThread(). Every Ice method used from
    // this thread (writeBatch(), writePacket()) is thread-safe and
    // hops to the socket thread on its own, so nothing needs the two to match.

    // A previous ICE may have stopped this consumer before losing the session. This reset must
    // stay synchronous: posted, it could run after a stop issued by this same ICE has raised the
    // flag and queued its cleanup, and that cleanup would then skip itself.
    m_needStop = false;
    // m_streamer is read in the session thread, which is not this one for IceTcp.
    runInMuxingThread([this, streamer]() { m_streamer = streamer; });
}

void Consumer::stopUnsafe()
{
    m_needStop = true;

    // Called from the ICE thread, which is the session thread for every ICE but IceTcp.
    m_pollable.dispatch(
        [this]()
        {
            if (!m_needStop)
                return; //< Another ICE has locked the session and restarted the stream meanwhile.
            m_pollable.pleaseStopSync();
        });
}

void Consumer::startProvidersIfNeeded()
{
    if (!NX_ASSERT(m_session))
        return;
    if (!m_streamer)
        return; //< It is not started yet.
    runInMuxingThread(
        [this]()
        {
            for (auto& provider: m_session->allProviders())
            {
                if (!provider->isStarted())
                {
                    m_undecryptableTimestampsUs.erase(provider->resource()->getId());
                    provider->play(provider->getPosition(), false);
                }
            }
        });
}

void Consumer::seek(nx::Uuid deviceId, int64_t timestampUs, StatusHandler handler)
{
    if (!NX_ASSERT(m_session))
        return;

    runInMuxingThread(
        [this, handler, deviceId, timestampUs]()
        {
            auto doSeek = [this, timestampUs](nx::Uuid id, auto provider, auto handler)
            {
                if (provider && !m_session->isStreamedCodecNegotiated(id))
                {
                    // The client took the track but not the codec the Device streams on it, and
                    // no offer of ours can hand it one it cannot decode. Every packet would go
                    // out under a payload type the receiver drops, so the Device stops here and
                    // the client is told why, as it is when a codec change ends up outside the
                    // answer, see Session::switchCodec().
                    NX_WARNING(
                        this, "The client did not take the codec device %1 streams, stop it", id);
                    provider->suspend();
                    handler(id, StreamStatus::codecNotSupported);
                    return;
                }

                auto status = provider && provider->play(timestampUs, /*useAudioLastGop*/ false)
                    ? StreamStatus::success
                    : StreamStatus::nodata;
                handler(id, status);
            };

            if (!deviceId.isNull())
            {
                m_undecryptableTimestampsUs.erase(deviceId);
                doSeek(deviceId, m_session->provider(deviceId), handler);
            }
            else
            {
                m_undecryptableTimestampsUs.clear();
                for (const auto& provider: m_session->allProviders())
                    doSeek(provider->resource()->getId(), provider, handler);
            }
        });
}

void Consumer::changeQuality(
    nx::Uuid deviceId,
    nx::vms::api::StreamIndex stream,
    StatusHandler handler)
{
    if (!NX_ASSERT(m_session))
        return;

    runInMuxingThread(
        [this, handler, deviceId, stream]()
        {
            auto doChangeQuality = [](nx::Uuid id, auto provider, auto stream, auto handler)
            {
                auto status = provider && provider->changeQuality(stream) ? StreamStatus::success
                                                                          : StreamStatus::nodata;
                handler(id, status);
            };

            if (!deviceId.isNull())
            {
                doChangeQuality(deviceId, m_session->provider(deviceId), stream, handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doChangeQuality(provider->resource()->getId(), provider, stream, handler);
            }
        });
}

void Consumer::pause(nx::Uuid deviceId, StatusHandler handler)
{
    if (!NX_ASSERT(m_session))
        return;

    runInMuxingThread(
        [this, handler = std::move(handler), deviceId]()
        {
            auto doPause = [](nx::Uuid id, auto provider, auto handler)
            {
                auto status =
                    provider && provider->pause() ? StreamStatus::success : StreamStatus::nodata;
                handler(id, status);
            };

            if (!deviceId.isNull())
            {
                doPause(deviceId, m_session->provider(deviceId), handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doPause(provider->resource()->getId(), provider, handler);
            }
        });
}

void Consumer::resume(nx::Uuid deviceId, StatusHandler handler)
{
    runInMuxingThread(
        [this, handler = std::move(handler), deviceId]()
        {
            auto doResume = [](nx::Uuid id, auto provider, auto handler)
            {
                auto status =
                    provider && provider->resume() ? StreamStatus::success : StreamStatus::nodata;
                handler(id, status);
            };

            if (!deviceId.isNull())
            {
                doResume(deviceId, m_session->provider(deviceId), handler);
            }
            else
            {
                for (const auto& provider: m_session->allProviders())
                    doResume(provider->resource()->getId(), provider, handler);
            }
        });
}

void Consumer::nextFrame(nx::Uuid deviceId, StatusHandler handler)
{
    if (!NX_ASSERT(m_session))
        return;

    runInMuxingThread(
        [this, handler, deviceId]()
        {
            auto provider = m_session->provider(deviceId);
            auto status = provider && provider->nextFrame()
                ? StreamStatus::success
                : StreamStatus::nodata;
            handler(deviceId, status);
        });
}

bool Consumer::handleSrtp(std::vector<uint8_t> buffer)
{
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
    auto decryptor = m_session->muxer()->getDecryptor();
    const auto decryptResult =
        decryptor ? decryptor->decryptPacket(data, &size) : rtsp::SrtpDecryptor::Result::success;
    if (decryptResult != rtsp::SrtpDecryptor::Result::success)
    {
        if (decryptResult == rtsp::SrtpDecryptor::Result::packetRejected)
            NX_VERBOSE(this, "Discard rejected SRTCP packet");
        else
            NX_WARNING(this, "Failed to decrypt SRTCP packet");
        return;
    }

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
    while (!m_needStop)
    {
        auto media = m_mediaQueue.popData(false /*blocked*/);
        if (!media)
            break;

        NX_VERBOSE(this, "Process data: %1", media);

        // Send metadata via datachannel.
        const auto metadata = std::dynamic_pointer_cast<const QnCompressedMetadata>(media);
        if (metadata && metadata->metadataType == MetadataType::ObjectDetection)
        {
            sendMetadata(metadata);
            return;
        }

        if (metadata && metadata->metadataType == MetadataType::MediaStreamEvent)
        {
            if (metadata->toMediaEventPacket().code != nx::media::StreamEvent::cannotDecryptMedia)
                continue;

            // Report each packet once, see m_undecryptableTimestampsUs.
            const auto [reported, isNew] =
                m_undecryptableTimestampsUs.try_emplace(metadata->deviceId, metadata->timestamp);
            if (!isNew && reported->second == metadata->timestamp)
                continue;

            reported->second = metadata->timestamp;
            nx::reflect::json::SerializationContext ctx;
            ctx.flags = 0; //< Device id without braces, as in the other messages.
            ctx.composer.startObject();
            ctx.composer.writeAttributeName("cannotDecryptMedia");
            nx::reflect::json::serialize(&ctx,
                CannotDecryptMedia{.deviceId = metadata->deviceId,
                    .timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::microseconds(metadata->timestamp))});
            ctx.composer.endObject(1);
            m_streamer->ice()->writeDataChannelString(ctx.composer.take());
            continue;
        }

        const auto deviceId = media->deviceId;

        auto rtpEncoder = m_session->muxer()->setDataPacket(deviceId, media);
        if (!rtpEncoder)
        {
            NX_VERBOSE(this, "Skip data: %1", media);
            continue;
        }

        nx::utils::ByteArray buffer;
        std::deque<nx::Buffer> mediaBuffers;
        while (!m_needStop && rtpEncoder->getNextPacket(buffer))
        {
            mediaBuffers.emplace_back(buffer.data(), buffer.size());
            buffer.clear();
        }

        // In case with changed codec, encoder will report with EOF.
        // Maybe need to make additional check for extradata: for example, some high h.264 profiles
        // can be unsupported by browser.
        if (rtpEncoder->isEof())
        {
            // The m-line offers every codec its Device may switch to, so a codec change costs no
            // SDP at all: the encoder is rebuilt around the new codec and goes on streaming on
            // the same m-line, under a payload type the client has already accepted.
            switch (m_session->switchCodec(deviceId, media->dataType))
            {
                case CodecSwitchResult::done:
                    // This packet is the first one of the new codec, and a key frame: it is what
                    // the client needs to start decoding, so packetize it after all.
                    rtpEncoder = m_session->muxer()->setDataPacket(deviceId, media);
                    if (!rtpEncoder)
                        continue;

                    mediaBuffers.clear();
                    while (!m_needStop && rtpEncoder->getNextPacket(buffer))
                    {
                        mediaBuffers.emplace_back(buffer.data(), buffer.size());
                        buffer.clear();
                    }
                    break;

                case CodecSwitchResult::notNegotiated:
                    // No offer of ours can hand the client a codec it cannot decode, so there
                    // is nothing left to try: this Device stops and the client is told why.
                    // Anything still queued for it would only be dropped.
                    NX_WARNING(this,
                        "Device %1 switched to a codec the client did not take, stop it",
                        deviceId);
                    if (auto provider = m_session->provider(deviceId))
                        provider->suspend();
                    clearQueueForDevice(deviceId);
                    m_streamer->handleStreamStatus(deviceId, StreamStatus::codecNotSupported);
                    return;

                case CodecSwitchResult::failed:
                    NX_WARNING(this,
                        "Failed to switch the %1 codec of device %2, ask the client to reconnect",
                        media->dataType,
                        deviceId);
                    m_streamer->handleStreamStatus(deviceId, StreamStatus::reconnect);
                    return;
            }
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

            m_streamer->ice()->writeBatch(mediaBuffers);
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

std::string Consumer::idForToStringFromPtr() const
{
    return m_sessionId;
}

} // namespace nx::webrtc
