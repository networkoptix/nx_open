#include "session.h"

#include <random>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/network/http/custom_headers.h>
#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/random_qt_device.h>

void Session::run(const QString& url, const Config& config, bool live)
{
    static const int kTcpPrefixLength = 4;
    int64_t position = DATETIME_NOW;
    if (!live)
    {
        nx::utils::random::QtDevice rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(60, 3600);
        position = QDateTime::currentMSecsSinceEpoch() * 1000 - dis(gen) * 1000000;
    }
    NX_INFO(this, "Start test rtps session: %1 %2",
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
                if (!processPacket(data, size, url.toUtf8().data()))
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
            printf("WARNING: Failed to read data from camera %s", url.toUtf8().data());
            failed = true;
            ++failedCount;
            break;
        }
        totalBytesRead += bytesRead;
    }
    for (auto& data: dataArrays)
        delete data;
}

bool Session::processPacket(const uint8_t* data, int64_t size, const char* url)
{
    int64_t timestamp = parsePacketTimestamp(data, size);

    auto nowTime = std::chrono::system_clock::now();
    if (timestamp != -1)
    {
        if (m_config.printTimestamps)
        {
            printf("Camera %s timestamp %lld us\n", url,
                (long long int) timestamp);
        }
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

int64_t Session::parsePacketTimestamp(const uint8_t* data, int64_t size)
{
    using namespace nx::streaming::rtp;
    static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;

    const RtpHeader* rtpHeader = (RtpHeader*) data;
    if (rtpHeader->CSRCCount != 0 || rtpHeader->version != 2)
    {
        NX_WARNING(this, "Got malformed RTP packet header. Ignored.");
        return -1;
    }

    const quint8* payload = data + RtpHeader::kSize;
    size -= RtpHeader::kSize;
    // Odd numbers - codec context, even numbers - data. Ignore context
    if ((qFromBigEndian(rtpHeader->ssrc) & 0x01) != 0)
        return -1;

    if (rtpHeader->padding)
        size -= qFromBigEndian(rtpHeader->padding);

    if (m_newPacket)
    {
        if (size < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
            return -1;

        const QnAbstractMediaData::DataType dataType = (QnAbstractMediaData::DataType)*(payload++);
        const quint32 timestampHigh = qFromBigEndian(*(quint32*) payload);
        const int64_t timestamp = qFromBigEndian(rtpHeader->timestamp) +
            (qint64(timestampHigh) << 32);
        if (dataType == QnAbstractMediaData::VIDEO)
            return timestamp;
    }
    m_newPacket = rtpHeader->marker;
    return -1;
}

