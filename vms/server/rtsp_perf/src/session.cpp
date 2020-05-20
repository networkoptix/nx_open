#include "session.h"

#include <random>
#include <iomanip>
#include <chrono>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/network/http/custom_headers.h>
#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/random_qt_device.h>

#include "report.h"

static const char* streamTypeToString(uint16_t flags)
{
    const auto mediaFlags = static_cast<QnAbstractMediaData::MediaFlags>(flags);
    if (mediaFlags.testFlag(QnAbstractMediaData::MediaFlags_LowQuality))
        return "secondary";
    return "primary";
}

void Session::run(const QString& url, const Config& config, bool live)
{
    static const int kTcpPrefixLength = 4;
    m_config = config;

    m_prevTimestampUs = -1;
    m_prevSequence = (uint16_t) -1; //< The sequence number of the first packet will be 0.
    m_newPacket = true;
    m_lastFrameTime.reset();
    int64_t positionUs = DATETIME_NOW;
    if (!live)
    {
        if (m_config.archivePosition != std::chrono::milliseconds(-1))
        {
            positionUs = std::chrono::duration_cast<std::chrono::microseconds>(
                m_config.archivePosition).count();
        }
        else
        {
            nx::utils::random::QtDevice rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(60, 3600);
            positionUs = QDateTime::currentMSecsSinceEpoch() * 1000 - dis(gen) * 1000000ll;
        }
    }
    NX_INFO(this, "Start test rtsp session: %1 %2",
        url,
        live ? "live" : "archive, from position: " + QString::number(positionUs / 1000) + "ms");

    QAuthenticator auth;
    auth.setUser(config.user);
    auth.setPassword(config.password);
    QnRtspClient::Config rtspConfig;
    QnRtspClient rtspClient(rtspConfig);
    rtspClient.setTCPTimeout(config.timeout);
    rtspClient.setAuth(auth, nx::network::http::header::AuthScheme::basic);
    rtspClient.setTransport(nx::vms::api::RtpTransportType::tcp);
    rtspClient.setAdditionAttribute(Qn::EC2_INTERNAL_RTP_FORMAT, "1");
    rtspClient.setAdditionAttribute("Speed", "1");
    rtspClient.setAdditionAttribute("x-fast-channel-zapping", "false");

    CameraDiagnostics::Result result = rtspClient.open(url);
    if (result.errorCode != 0)
    {
        report("Failed to open rtsp stream: %1, error: %2",
            url, result.toString(/*resourcePool*/ nullptr));
        failed = true;
        ++failedCount;
        return;
    }
    rtspClient.play(positionUs, AV_NOPTS_VALUE, 1.0);
    int channel = -1;
    std::vector<QnByteArray*> dataArrays;
    for (;;)
    {
        const int bytesRead = rtspClient.readBinaryResponse(dataArrays, channel);
        if (channel >= 0 && (int)dataArrays.size() > channel && dataArrays[channel])
        {
            if (bytesRead > 0)
            {
                uint8_t* data = (uint8_t*)dataArrays[channel]->data() + kTcpPrefixLength;
                int64_t size = dataArrays[channel]->size() - kTcpPrefixLength;
                if (channel == 0 && !processPacket(data, size, url.toUtf8().data()))
                {
                    failed = true;
                    ++failedCount;
                    break;
                }
            }
            dataArrays[channel]->clear();
        }
        if (bytesRead <= 0)
        {
            report("WARNING: Camera %1: Failed to read data.", url);
            failed = true;
            ++failedCount;
            break;
        }
        totalBytesRead += bytesRead;
    }
    for (auto& data: dataArrays)
        delete data;
}

void Session::checkDiff(std::chrono::microseconds diff, int64_t timestampUs, const char* url)
{
    constexpr auto kZeroUs = std::chrono::microseconds::zero();
    if (m_config.maxTimestampDiff != kZeroUs && diff > m_config.maxTimestampDiff)
    {
        report("WARNING: Camera %1: Frame timestamp %2 us: diff %3 us is more than %4 us.",
            url, timestampUs, diff.count(), m_config.maxTimestampDiff.count());
    }
    else if (m_config.minTimestampDiff != kZeroUs && diff < m_config.minTimestampDiff)
    {
        report("WARNING: Camera %1: Frame timestamp %2 us: diff %3 us is less than %4 us.",
            url, timestampUs, diff.count(), m_config.minTimestampDiff.count());
    }
}

bool Session::processPacket(const uint8_t* data, int64_t size, const char* url)
{
    const auto nowTime = std::chrono::steady_clock::now();
    Packet packet;
    if (parsePacket(data, size, packet, url))
    {
        if (m_config.printTimestamps)
        {
            const int64_t timestampDiffUs = packet.timestampUs - m_prevTimestampUs;
            report("Camera %1: Frame timestamp %2 us, diff %3 us, stream %4.",
                url,
                packet.timestampUs,
                (m_prevTimestampUs == -1) ? -1 : timestampDiffUs,
                streamTypeToString(packet.flags));

            if (m_prevTimestampUs != -1)
                checkDiff(std::chrono::microseconds(timestampDiffUs), packet.timestampUs, url);
        }
        m_prevTimestampUs = packet.timestampUs;
        m_lastFrameTime = nowTime;
    }
    else
    {
        if (m_lastFrameTime)
        {
            const std::chrono::duration<double> timeFromLastFrame = nowTime - *m_lastFrameTime;
            if (timeFromLastFrame > m_config.timeout)
            {
                report("WARNING: Camera %1: Video frame was not received for %2 s.",
                    url, timeFromLastFrame.count());
                return false;
            }
        }
    }
    return true;
}

bool Session::parsePacket(const uint8_t* data, int64_t size, Packet& packet, const char* url)
{
    using namespace nx::streaming::rtp;
    static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;

    if (size < sizeof(RtpHeader))
    {
        NX_WARNING(this, "Camera %1: Got too short RTP packet. Ignored.", url);
        return false;
    }

    const RtpHeader* rtpHeader = (RtpHeader*) data;
    if (rtpHeader->CSRCCount != 0 || rtpHeader->version != 2)
    {
        NX_WARNING(this, "Camera %1: Got malformed RTP packet header. Ignored.", url);
        return false;
    }

    const uint16_t sequence = qFromBigEndian(rtpHeader->sequence);
    if (!m_config.ignoreSequenceNumberErrors && sequence != (uint16_t) (m_prevSequence + 1))
    {
        NX_WARNING(this, "Camera %1: Unexpected RTP packet sequence number: %2 instead of %3.",
            url, sequence, m_prevSequence);
    }
    m_prevSequence = sequence;

    const bool isNewPacket = m_newPacket;
    m_newPacket = rtpHeader->marker;

    const quint8* payload = data + RtpHeader::kSize;
    size -= RtpHeader::kSize;
    // Odd numbers - codec context, even numbers - data. Ignore context
    if ((qFromBigEndian(rtpHeader->ssrc) & 0x01) != 0)
        return false;

    if (rtpHeader->padding)
        size -= qFromBigEndian(rtpHeader->padding);

    if (isNewPacket)
    {
        if (size < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
            return false;

        const QnAbstractMediaData::DataType dataType = (QnAbstractMediaData::DataType)*(payload++);
        const uint32_t timestampHigh = qFromBigEndian(*(uint32_t*) payload);
        packet.timestampUs = qFromBigEndian(rtpHeader->timestamp) + (int64_t(timestampHigh) << 32);
        payload += 5;
        packet.flags = (payload[0] << 8) + payload[1];
        if (dataType == QnAbstractMediaData::VIDEO)
            return true;
    }
    return false;
}

