// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_demuxer.h"

#include <core/resource/camera_resource.h>
#include <nx/rtp/parsers/h264_rtp_parser.h>
#include <nx/rtp/parsers/hevc_rtp_parser.h>
#include <nx/rtp/parsers/i_rtp_parser_factory.h>
#include <nx/rtp/parsers/mjpeg_rtp_parser.h>
#include <nx/rtp/parsers/simpleaudio_rtp_parser.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

namespace nx::webrtc {

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
    nx::Uuid deviceId;

    nx::rtp::Sdp sdp;
    sdp.parse(QString::fromStdString(value));
    for (const auto& i: sdp.media)
    {
        nx::rtp::StreamParserPtr parser;
        // TODO should check mediaType too. SDP parser parse `codecName` of `m=application` track as PCMU.
        if (i.mediaType == nx::rtp::Sdp::MediaType::Video)
        {
            NX_VERBOSE(this, "Check video codec: %1", i.rtpmap.codecName.toStdString());
            if (i.rtpmap.codecName == "H264")
            {
                parser = std::make_unique<nx::rtp::H264Parser>();
            }
            else if (i.rtpmap.codecName == "H265")
            {
                parser = std::make_unique<nx::rtp::HevcParser>();
            }
            else if (i.rtpmap.codecName == "JPEG")
            {
                parser = std::make_unique<nx::rtp::MjpegParser>();
            }
        }
        else if (i.mediaType == nx::rtp::Sdp::MediaType::Audio)
        {
            NX_VERBOSE(this, "Check audio codec: %1", i.rtpmap.codecName.toStdString());
            if (i.rtpmap.codecName == "PCMU")
            {
                parser = std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_MULAW);
            }
            else if (i.rtpmap.codecName == "PCMA")
            {
                parser = std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_ALAW);
            }
            else if (i.rtpmap.codecName.startsWith("G726")) //< g726-24, g726-32 etc.
            {
                int bitRatePos = i.rtpmap.codecName.indexOf(QLatin1Char('-'));
                if (bitRatePos == -1)
                {
                    NX_VERBOSE(this, "For G726 codec bitrate is not specified: %1", i.rtpmap.codecName.toStdString());
                    continue;
                }
                QString bitsPerSample = i.rtpmap.codecName.mid(bitRatePos + 1);
                auto audioParser = std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_ADPCM_G726);
                audioParser->setBitsPerSample(bitsPerSample.toInt() / 8);
                parser = std::move(audioParser);
            }
            else if (i.rtpmap.codecName == "L16")
            {
                parser = std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_S16BE);
            }
        }
        else
        {
            NX_VERBOSE(this, "Skipping non-media track: %1", i.toString());
            continue;
        }
        if (!parser)
        {
            NX_VERBOSE(this, "Can't create parser for codec: %1", i.rtpmap.codecName.toStdString());
            continue;
        }

        uint32_t senderSsrc = 0;
        parser->setSdpInfo(i);
        if (dynamic_cast<nx::rtp::VideoStreamParser*>(parser.get()) != nullptr)
        {
            m_rtcpFir.sourceSsrc = i.ssrc;
            if (const Track* track = m_tracks->videoTrack(deviceId))
                senderSsrc = track->ssrc;
        }
        if (auto audioParser = dynamic_cast<nx::rtp::AudioStreamParser*>(parser.get());
            audioParser != nullptr)
        {
            m_audioCodecParameters = audioParser->getCodecParameters();
            if (const Track* track = m_tracks->audioTrack(deviceId))
                senderSsrc = track->ssrc;
        }
        NX_ASSERT(m_resource);
        auto timeHelper = std::make_unique<nx::streaming::rtp::CameraTimeHelper>(
            i.mediaType,
            m_resource->getPhysicalId().toStdString(),
            m_resource->getTimeOffset());
        auto result = m_parsers.emplace(
            i.ssrc,
            ParserContext(
                nx::rtp::RtpParser(i.payloadType, std::move(parser)),
                senderSsrc,
                std::move(timeHelper)));
        if (!result.second)
        {
            NX_DEBUG(this, "Track with ssrc: %1 ptype: %2 codec: %3 is not inserted",
                i.ssrc, i.payloadType, i.rtpmap.codecName);
        }
        m_reorderers[i.ssrc];
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
    const nx::rtp::RtpHeader* header = (nx::rtp::RtpHeader*) array.data();
    if (header->isRtcp())
    {
        return processRtcp(array);
    }
    else
    {
        return processRtp(array);
    }
}

bool Demuxer::processRtp(const nx::utils::ByteArray& array)
{
    const nx::rtp::RtpHeader* header = (nx::rtp::RtpHeader*) array.data();
    uint32_t ssrc = ntohl(header->ssrc);
    if (m_parsers.find(ssrc) == m_parsers.end())
    {
        NX_DEBUG(this, "Got unknown SSRC: %1", ssrc);
        return true;
    }
    auto reorderer = m_reorderers.find(ssrc);
    NX_ASSERT(reorderer != m_reorderers.end());

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
    const nx::rtp::RtpHeader* header = (const nx::rtp::RtpHeader*) data;
    NX_ASSERT(!header->isRtcp());
    uint32_t ssrc = ntohl(header->ssrc);
    auto parser = m_parsers.find(ssrc);
    NX_ASSERT(parser != m_parsers.end());

    auto& buffer = parser->second.buffer;
    auto oldSize = buffer.size();
    buffer.insert(
        buffer.end(),
        (const uint8_t*) data,
        (const uint8_t*) data + size);

    bool packetLoss = false, gotData = false;

    parser->second.parser.processData(
        buffer.data(), oldSize, buffer.size() - oldSize, packetLoss, gotData);
    if (gotData)
    {
        nx::rtp::RtcpSenderReport senderReport;
        auto media = parser->second.parser.nextData(senderReport);
        media->timestamp = parser->second.timeHelper->getTime(
            qnSyncTime->currentTimePoint(),
            std::chrono::microseconds(media->timestamp),
            parser->second.parser.isUtcTime(),
            true).count();
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
