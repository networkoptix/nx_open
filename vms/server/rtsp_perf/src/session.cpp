#include "session.h"

#include <random>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/network/http/custom_headers.h>
#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/random_qt_device.h>


const char* streamTypeToString(uint16_t flags)
{
    auto mediaFlags = static_cast<QnAbstractMediaData::MediaFlags>(flags);
    if (mediaFlags.testFlag(QnAbstractMediaData::MediaFlags_LowQuality))
        return "low quality";
    return "high quality";
}

void Session::run(const QString& url, const Config& config, bool live)
{
    static const int kTcpPrefixLength = 4;
    int64_t position = DATETIME_NOW;
    if (!live)
    {
        nx::utils::random::QtDevice rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(60, 3600);
        position = QDateTime::currentMSecsSinceEpoch() * 1000 - dis(gen) * 1000000ll;
    }
    NX_INFO(this, "Start test rtsp session: %1 %2",
        url,
        live ? "live" : "archive, from position: " + QString::number(position / 1000000));

    m_config = config;
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
    CameraDiagnostics::Result result = rtspClient.open(url);
    if (result.errorCode != 0)
    {
        NX_ERROR(this, "Failed to open rtsp stream: %1", result.toString(nullptr));
        failed = true;
        ++failedCount;
        return;
    }
    rtspClient.play(position, AV_NOPTS_VALUE, 1.0);
    int channel = -1;
    std::vector<QnByteArray*> dataArrays;
    m_lastFrameTime = std::chrono::system_clock::now();
    while (true)
    {
        int bytesRead = rtspClient.readBinaryResponce(dataArrays, channel);
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
            printf("WARNING: Camera %s: failed to read data\n", url.toUtf8().data());
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
    using namespace std::chrono;
    if (m_config.maxTimestampDiff != milliseconds::zero() && diff > m_config.maxTimestampDiff)
    {
        printf("WARNING: Camera %s: frame timestamp %lld us: diff %lld us more than %lld us\n",
            url,
            (long long int) timestampUs,
            (long long int) diff.count(),
            (long long int) m_config.maxTimestampDiff.count());
    }
    if (m_config.minTimestampDiff != milliseconds::zero() && diff < m_config.minTimestampDiff)
    {
        printf("WARNING: Camera %s: frame timestamp %lld us: diff %lld us is less than %lld us\n",
            url,
            (long long int) timestampUs,
            (long long int) diff.count(),
            (long long int) m_config.minTimestampDiff.count());
    }
}

bool Session::processPacket(const uint8_t* data, int64_t size, const char* url)
{
    auto nowTime = std::chrono::system_clock::now();
    Packet packet;
    if (parsePacket(data, size, packet))
    {
        if (m_config.printTimestamps)
        {
            int64_t timestampDiffUs = packet.timestampUs - m_prevTimestampUs;
            printf("Camera %s: timestamp %lld us, diff %lld us, stream type %s\n", url,
                (long long int) packet.timestampUs,
                (m_prevTimestampUs == -1) ? -1LL : timestampDiffUs,
                streamTypeToString(packet.flags));

            if (m_prevTimestampUs != -1)
                checkDiff(std::chrono::microseconds(timestampDiffUs), packet.timestampUs, url);
        }
        m_prevTimestampUs = packet.timestampUs;
        m_lastFrameTime = nowTime;

    }
    else
    {
        std::chrono::duration<double> timeFromLastFrame = nowTime - m_lastFrameTime;
        if (timeFromLastFrame > m_config.timeout)
        {
            printf("WARNING: camera %s, video frame was not received for %lfsec\n",
                url, timeFromLastFrame.count());
            return false;
        }
    }
    return true;
}

bool Session::parsePacket(const uint8_t* data, int64_t size, Packet& packet)
{
    using namespace nx::streaming::rtp;
    static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;

    if (size < sizeof(RtpHeader))
    {
        NX_WARNING(this, "Got too short RTP packet. Ignored.");
        return false;
    }

    const RtpHeader* rtpHeader = (RtpHeader*) data;
    if (rtpHeader->CSRCCount != 0 || rtpHeader->version != 2)
    {
        NX_WARNING(this, "Got malformed RTP packet header. Ignored.");
        return false;
    }

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

