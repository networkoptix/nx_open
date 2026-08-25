// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_demuxer.h"

#include <array>
#include <chrono>

#include <core/resource/camera_resource.h>
#include <nx/rtp/parsers/av1_rtp_parser.h>
#include <nx/rtp/parsers/h264_rtp_parser.h>
#include <nx/rtp/parsers/hevc_rtp_parser.h>
#include <nx/rtp/parsers/i_rtp_parser_factory.h>
#include <nx/rtp/parsers/mjpeg_rtp_parser.h>
#include <nx/rtp/parsers/opus_rtp_parser.h>
#include <nx/rtp/parsers/simpleaudio_rtp_parser.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

namespace nx::webrtc {

namespace {

// Codec names are case-insensitive per RFC 4566; e.g. browsers spell this one "opus" while others
// spell it "OPUS".
bool sameCodec(const QString& sdpCodecName, const char* codecName)
{
    return sdpCodecName.compare(codecName, Qt::CaseInsensitive) == 0;
}

// G726 carries its bitrate in the codec name: g726-16, g726-24, g726-32, g726-40. Returns null if
// the name has no bitrate suffix, which leaves the track without a parser and so skipped.
std::unique_ptr<nx::rtp::AudioStreamParser> makeG726Parser(const QString& codecName)
{
    const int bitRatePos = codecName.indexOf(QLatin1Char('-'));
    if (bitRatePos == -1)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "For G726 codec bitrate is not specified: %1", codecName);
        return nullptr;
    }

    auto parser = std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_ADPCM_G726);
    parser->setBitsPerSample(codecName.mid(bitRatePos + 1).toInt() / 8);
    return parser;
}

// Kept split by media type: setSdp() needs the audio parser typed to read its codec parameters,
// and so an audio m-line must never yield a VideoStreamParser.
nx::rtp::StreamParserPtr makeVideoParser(const QString& codec)
{
    NX_VERBOSE(NX_SCOPE_TAG, "SDP video codec: %1", codec);
    if (sameCodec(codec, "H264"))
        return std::make_unique<nx::rtp::H264Parser>();
    if (sameCodec(codec, "H265"))
        return std::make_unique<nx::rtp::HevcParser>();
    if (sameCodec(codec, "JPEG"))
        return std::make_unique<nx::rtp::MjpegParser>();
    if (sameCodec(codec, "AV1") || sameCodec(codec, "AV1X"))
        return std::make_unique<nx::rtp::Av1Parser>();
    return nullptr;
}

std::unique_ptr<nx::rtp::AudioStreamParser> makeAudioParser(const QString& codec)
{
    NX_VERBOSE(NX_SCOPE_TAG, "SDP audio codec: %1", codec);
    if (sameCodec(codec, "opus"))
        return std::make_unique<nx::rtp::OpusParser>();
    if (sameCodec(codec, "PCMU"))
        return std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_MULAW);
    if (sameCodec(codec, "PCMA"))
        return std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_ALAW);
    if (codec.startsWith("G726", Qt::CaseInsensitive))
        return makeG726Parser(codec); //< g726-24, g726-32 etc.
    if (sameCodec(codec, "L16"))
        return std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_S16BE);
    return nullptr;
}

} // namespace

Demuxer::Demuxer(const Tracks* tracks): m_tracks(tracks)
{
}

Demuxer::~Demuxer()
{
}

void Demuxer::setSrtpEncryptionData(const rtsp::EncryptionData& data)
{
    m_encryptor = std::make_unique<rtsp::SrtpEncryptor>();
    if (!m_encryptor->init(data))
    {
        NX_WARNING(this, "Failure to init SRTP encryptor");
        m_encryptor.reset();
    }
}

void Demuxer::setSdp(const std::string& value)
{
    // TODO: extract deviceId from SDP
    // Currently irrelevant: a Demuxer only ever serves one device's recv session, so the
    // videoTrack()/audioTrack() lookups below match on trackType alone regardless.
    nx::Uuid deviceId;

    nx::rtp::Sdp sdp;
    sdp.parse(QString::fromStdString(value));

    // Outer loop over media types, not over sdp.media: the recorder allocates all video streams
    // before the audio ones, and trackIdx has to end up matching that layout.
    constexpr std::array kTrackTypes = {
        nx::rtp::Sdp::MediaType::Video, nx::rtp::Sdp::MediaType::Audio};
    int trackIdx = 0;
    for (const auto trackType: kTrackTypes)
    {
        const bool video = trackType == nx::rtp::Sdp::MediaType::Video;
        for (const auto& media: sdp.media)
        {
            if (trackType != media.mediaType)
                continue;

            nx::rtp::StreamParserPtr parser;
            if (video)
            {
                parser = makeVideoParser(media.rtpmap.codecName);
                if (parser)
                    parser->setSdpInfo(media);
            }
            else if (auto audio = makeAudioParser(media.rtpmap.codecName))
            {
                // getCodecParameters() is only valid once setSdpInfo() has run.
                audio->setSdpInfo(media);
                m_audioCodecParameters = audio->getCodecParameters();
                parser = std::move(audio);
            }
            if (!parser)
            {
                NX_VERBOSE(this, "Can't create parser for codec: %1", media.rtpmap.codecName);
                continue;
            }

            uint32_t senderSsrc = 0;
            if (video)
            {
                NX_ASSERT(trackIdx == 0, "Only one video track is supported");
                m_hasVideo = true;
                m_rtcpFir.sourceSsrc = media.ssrc;
                m_videoMediaSsrc = media.ssrc;
                if (const Track* track = m_tracks->videoTrack(deviceId))
                {
                    senderSsrc = track->ssrc;
                    m_twcc.setSsrcs(media.ssrc, track->ssrc);
                }
            }
            else
            {
                m_hasAudio = true;
                if (const Track* track = m_tracks->audioTrack(deviceId))
                    senderSsrc = track->ssrc;
            }

            NX_ASSERT(m_resource);
            auto timeHelper =
                std::make_unique<nx::streaming::rtp::CameraTimeHelper>(media.mediaType,
                    m_resource->getPhysicalId().toStdString(),
                    m_resource->getTimeOffset());
            auto result = m_parsers.emplace(media.ssrc,
                ParserContext(nx::rtp::RtpParser(media.payloadType, std::move(parser)),
                    senderSsrc,
                    std::move(timeHelper),
                    trackIdx,
                    media.payloadType));
            if (result.second)
            {
                // Only a stored track consumes an index, otherwise the rest would no longer line
                // up with the recorder's streams. The publisher sends the payload type it listed
                // first, so this is the only codec this track will carry.
                ++trackIdx;
                NX_DEBUG(this,
                    "Track ssrc: %1 index: %2 codec: %3 ptype: %4",
                    media.ssrc,
                    result.first->second.trackIdx,
                    media.rtpmap.codecName,
                    media.payloadType);
            }
            else
            {
                NX_DEBUG(this,
                    "Track with ssrc: %1 ptype: %2 codec: %3 is not inserted",
                    media.ssrc,
                    media.payloadType,
                    media.rtpmap.codecName);
            }
            m_reorderers[media.ssrc];
        }
    }
}

bool Demuxer::processData(const char* data, size_t size)
{
    if (size < sizeof(nx::rtp::RtpHeader))
    {
        NX_VERBOSE(this, "Too small RTP packet: %1", size);
        return false;
    }
    nx::utils::ByteArray array(/*alignment*/ 1, size, /*padding*/ 1);
    array.write(data, size);

    if (m_encryptor)
    {
        int newSize = (int) size;
        m_encryptor->decryptPacket((uint8_t*) array.data(), &newSize);
        if (!NX_ASSERT(newSize <= (int) size))
            return false;
        array.resize(newSize);
    }

    auto header = reinterpret_cast<const nx::rtp::RtpHeader*>(array.data());
    if (header->isRtcp())
        return processRtcp(array);

    return processRtp(array);
}

bool Demuxer::processRtp(const nx::utils::ByteArray& array)
{
    auto header = reinterpret_cast<const nx::rtp::RtpHeader*>(array.data());
    uint32_t ssrc = ntohl(header->ssrc);
    if (m_parsers.find(ssrc) == m_parsers.end())
    {
        NX_DEBUG(this, "Got unknown SSRC: %1", ssrc);
        return true;
    }
    auto reorderer = m_reorderers.find(ssrc);
    NX_ASSERT(reorderer != m_reorderers.end());

    // Transport-wide congestion control: record the arrival timing of every
    // video RTP packet and echo it back as TWCC feedback. The publisher's
    // send-side estimator (libwebrtc GCC) runs on this feedback; without it the
    // publisher has no signal and stays at its conservative default bitrate.
    if (ssrc == m_videoMediaSsrc)
    {
        const auto arrival = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
        nx::rtp::RtpHeaderData hd;
        if (hd.read((const uint8_t*) array.data(), (int) array.size()) && hd.extension)
        {
            if (auto seq = parseTransportCcSeq(
                hd.extension->header.id(),
                (const uint8_t*) array.data() + hd.extension->extensionOffset,
                hd.extension->extensionSize,
                m_twcc.transportCcExtId()))
            {
                m_twcc.onRtpPacket(*seq, arrival);
            }
        }

        std::vector<nx::Buffer> feedbacks;
        m_twcc.maybeBuildFeedback(arrival, feedbacks);
        for (auto& fb: feedbacks)
        {
            encryptPacket(fb);
            m_feedbacks.emplace_back(std::move(fb));
        }
    }

    auto status = reorderer->second.pushPacket(array, header->getSequence());
    switch (status)
    {
        case nx::rtp::ReorderingCache::Status::pass:
        {
            return demux(array.constData(), array.size());
        }
        case nx::rtp::ReorderingCache::Status::flush:
        {
            NX_VERBOSE(this, "Reordering status: flush");
            nx::utils::ByteArray toSend;
            while (reorderer->second.getNextPacket(toSend))
            {
                if (!demux(toSend.constData(), toSend.size()))
                    return false;
                toSend.clear();
            }
            return true;
        }
        case nx::rtp::ReorderingCache::Status::wait:
        {
            NX_VERBOSE(this, "Reordering status: wait");
            auto nack = reorderer->second.getNextNackPacket(ssrc, ssrc); //< Seems to be the same.
            if (nack)
            {
                nx::Buffer buffer(nack->serialized(), 0);
                NX_ASSERT(nack->serialize((uint8_t*) buffer.data(), buffer.size()) ==
                    (int) buffer.size());
                encryptPacket(buffer);
                m_feedbacks.emplace_back(std::move(buffer));
            }
            return true;
        }
        case nx::rtp::ReorderingCache::Status::skip:
        {
            NX_VERBOSE(this, "Reordering status: skip");
            return true;
        }
        default:
        {
            NX_ASSERT(false, "Unexpected return status: %1", status);
            return false;
        }
    }

    return false;
}

bool Demuxer::processRtcp(const nx::utils::ByteArray& array)
{
    if (array.size() < nx::rtp::kRtcpHeaderSize)
        return false;
    switch ((uint8_t) array.data()[1])
    {
        case nx::rtp::kRtcpSenderReport:
        {
            uint32_t ssrc = nx::rtp::getRtcpSsrc((const uint8_t*) array.data(), array.size());
            auto parser = m_parsers.find(ssrc);
            if (parser == m_parsers.end())
                return true;
            nx::rtp::RtcpSenderReport senderReport;
            if (senderReport.read((const uint8_t*) array.data(), array.size()))
                parser->second.senderReport = senderReport;
            nx::Buffer output(nx::rtp::kRtcpReceiverReportLength, 0);
            int outBufSize = nx::rtp::buildReceiverReport(
                (uint8_t*) output.data(), output.size(), parser->second.senderSsrc);
            NX_ASSERT(outBufSize == nx::rtp::kRtcpReceiverReportLength);

            encryptPacket(output);

            m_feedbacks.emplace_back(std::move(output));
            return true;
        }
        default:
        {
            NX_VERBOSE(
                this,
                "Got RTCP packet with size %1 and type %2",
                array.size(),
                (uint8_t) array.data()[1]);
            return true;
        }
    }
}

bool Demuxer::demux(const char* data, size_t size)
{
    NX_ASSERT(size >= sizeof(nx::rtp::RtpHeader));
    auto header = reinterpret_cast<const nx::rtp::RtpHeader*>(data);
    NX_ASSERT(!header->isRtcp());
    uint32_t ssrc = ntohl(header->ssrc);
    auto parser = m_parsers.find(ssrc);
    NX_ASSERT(parser != m_parsers.end());
    auto& context = parser->second;

    if (header->payloadType != context.payloadType)
    {
        // RtpParser would skip it anyway, but only after buffering, and `buffer` is cleared only
        // when a frame comes out. Dropping here keeps it from growing for the whole session.
        if (context.reportedUnexpectedPayloadType != header->payloadType)
        {
            context.reportedUnexpectedPayloadType = header->payloadType;
            NX_WARNING(this,
                "Dropping ssrc %1 packets with payload type %2: the track was negotiated as "
                "payload type %3 and the codec cannot be changed mid-stream",
                ssrc,
                static_cast<int>(header->payloadType),
                context.payloadType);
        }
        context.parser.resetSequenceCheck();
        return true;
    }

    auto& buffer = context.buffer;
    auto oldSize = buffer.size();
    buffer.insert(
        buffer.end(),
        (const uint8_t*) data,
        (const uint8_t*) data + size);

    bool packetLoss = false, gotData = false;

    context.parser.processData(
        buffer.data(), oldSize, buffer.size() - oldSize, packetLoss, gotData);
    if (gotData)
    {
        nx::rtp::RtcpSenderReport senderReport;
        auto media = context.parser.nextData(senderReport);
        media->channelNumber = static_cast<uint32_t>(context.trackIdx);
        media->timestamp = context.timeHelper
                               ->getTime(qnSyncTime->currentTimePoint(),
                                   std::chrono::microseconds(media->timestamp),
                                   context.parser.isUtcTime(),
                                   true)
                               .count();
        // TODO emit network issues like in server_rtsp_stream_provider.cpp.
        // TODO Separate timestamp-related code.
        m_frames.emplace_back(std::move(media));
        buffer.clear(); //< Should be clear()ed only after got data.
    }
    return true;
}

nx::Buffer Demuxer::getNextOutput()
{
    nx::Buffer result;
    if (!m_feedbacks.empty())
    {
        result = std::move(m_feedbacks.front());
        m_feedbacks.pop_front();
    }
    return result;
}

QnAbstractMediaDataPtr Demuxer::getNextFrame()
{
    QnAbstractMediaDataPtr frame;
    if (!m_frames.empty())
    {
        frame = std::move(m_frames.front());
        m_frames.pop_front();
    }
    return frame;
}

nx::Buffer Demuxer::getRtcpFirPacket(uint32_t videoSsrc)
{
    if (!videoSsrc)
        return nx::Buffer();

    nx::Buffer result(nx::rtp::RtcpFirFeedback::size(), 0);
    if (!m_rtcpFir.getNextFeedback(videoSsrc, (uint8_t*) result.data(), result.size()))
        return result;

    encryptPacket(result);

    return result;
}

void Demuxer::encryptPacket(nx::Buffer& buffer)
{
    if (m_encryptor)
    {
        nx::utils::ByteArray array(/*alignment*/ 1, buffer.size(), /*padding*/ 1);
        array.write(buffer.data(), buffer.size());

        m_encryptor->encryptPacket(&array, /*offset*/ 0);
        buffer.clear();
        buffer.append(array.data(), array.size());
    }
}

} // namespace nx::webrtc
