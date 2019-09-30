#include "rtsp_perf.h"

#include <random>

#include <nx/streaming/rtsp_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/utils/random_qt_device.h>
#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/video_data_packet.h>

std::chrono::milliseconds kMinStartInterval{100};
std::chrono::milliseconds kMaxStartInterval{10000};

struct BitrateCounter
{
    BitrateCounter() : m_prevCheckTime(std::chrono::system_clock::now()) {}

    float update(int64_t totalBytesRead)
    {
        auto time = std::chrono::system_clock::now();
        float bitrate = float(totalBytesRead - m_prevBytesRead) * 8 /
            std::chrono::duration<float>(time - m_prevCheckTime).count() / 1000000;
        m_prevBytesRead = totalBytesRead;
        m_prevCheckTime = time;
        return bitrate;
    }

private:
    std::chrono::time_point<std::chrono::system_clock> m_prevCheckTime;
    int64_t m_prevBytesRead = 0;
};

bool RtspPerf::getCamerasUrls(const QString& server, std::vector<QString>& urls)
{
    std::vector<QString> result;
    nx::network::http::HttpClient httpClient;
    httpClient.setUserName(m_config.user);
    httpClient.setUserPassword(m_config.password);

    const QString request = "http://" + server + "/ec2/getCamerasEx";
    NX_INFO(this, "Obtain camera list using request: %1", request);
    if (!httpClient.doGet(request))
    {
        NX_ERROR(this, "Failed to get camera list");
        return false;
    }
    if (!httpClient.response() ||
        !nx::network::http::StatusCode::isSuccessCode(httpClient.response()->statusLine.statusCode))
    {
        NX_ERROR(this, "Bad response from server: %1", httpClient.response()->statusLine.toString());
        return false;
    }
    auto responseBody = httpClient.fetchEntireMessageBody();
    if (!responseBody.has_value())
    {
        NX_ERROR(this, "Failed to read response from server: %1", httpClient.lastSysErrorCode());
        return false;
    }

    nx::vms::api::CameraDataExList cameras =
        QJson::deserialized<nx::vms::api::CameraDataExList>(responseBody.value());

    if (cameras.empty())
    {
        NX_ERROR(this, "No camera found in response from server");
        return false;
    }

    for(const auto& camera: cameras)
    {
        urls.emplace_back(
            QString(m_config.useSsl ? "rtsps://" : "rtsp://") + server + "/" + camera.id.toString());
    }
    return true;
}

void RtspPerf::startSessionsThread(const std::vector<QString>& urls)
{
    while (true)
    {
        for (int i = 0; i < (int)m_sessions.size(); ++i)
        {
            if (m_sessions[i].failed)
            {
                bool live = i < m_config.count * m_config.livePercent / 100;
                QString url = urls[i % urls.size()];
                m_sessions[i].failed = false;
                if (m_sessions[i].worker.joinable())
                    m_sessions[i].worker.join();
                m_sessions[i].worker = std::thread([url, live, this, i]() {
                    m_sessions[i].run(url, m_config, live);
                });
                ++m_totalFailed;

                if (m_totalFailed > 0 && m_config.startInterval == std::chrono::milliseconds::zero())
                    m_currentStartInterval = std::min(m_currentStartInterval * 2, kMaxStartInterval);

                std::this_thread::sleep_for(m_currentStartInterval);
            }
        }
        std::this_thread::sleep_for(m_currentStartInterval);
    }
}

void RtspPerf::run()
{
    static const std::chrono::seconds kStatisticPrintInterval(1);
    nx::network::SocketGlobals::InitGuard socketInitializationGuard;
    std::vector<Session> sessions(m_config.count);
    std::vector<QString> urls;

    if (m_config.urls.isEmpty())
    {
        if (!getCamerasUrls(m_config.server, urls))
            return;
    }
    else
    {
        for(const auto& url: m_config.urls)
            urls.push_back(url);
    }

    m_totalFailed = -m_config.count;
    m_currentStartInterval = m_config.startInterval != std::chrono::milliseconds::zero() ?
        m_config.startInterval : kMinStartInterval;
    BitrateCounter bitrateCounter;
    auto startSessionThread = std::thread([urls, this]() { startSessionsThread(urls); });
    while (true)
    {
        std::this_thread::sleep_for(kStatisticPrintInterval);
        int64_t totalBytesRead = 0;
        int64_t successSessions = 0;
        for (const auto& session: m_sessions)
        {
            totalBytesRead += session.totalBytesRead;
            if (!session.failed)
                ++successSessions;
        }
        float bitrate = bitrateCounter.update(totalBytesRead);
        NX_INFO(this, "Total bitrate %1 MBit/s, working sessions %2, failed %3, bytes read %4",
            QString::number(bitrate, 'f', 3), successSessions, std::max<int64_t>(m_totalFailed, 0), totalBytesRead);
    }
}

void RtspPerf::Session::run(const QString& url, const Config& config, bool live)
{
    static const int kInterleavedRtpOverTcpPrefixLength = 4;
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

    const QString cameraId = url.endsWith('/') ?
        url.mid(url.left(url.size() - 1).lastIndexOf('/') + 1) : url.mid(url.lastIndexOf('/') + 1);
    if (cameraId.isEmpty())
        NX_ERROR(this, "Failed to parse camera id from url: %1", url);

    QAuthenticator auth;
    auth.setUser(config.user);
    auth.setPassword(config.password);
    QnRtspClient::Config rtspConfig;
    QnRtspClient rtspClient(rtspConfig);
    rtspClient.setTCPTimeout(config.timeout);
    rtspClient.setAuth(auth, nx::network::http::header::AuthScheme::basic);
    rtspClient.setTransport(nx::vms::api::RtpTransportType::tcp);
    rtspClient.setAdditionAttribute(Qn::EC2_INTERNAL_RTP_FORMAT, "1");
    CameraDiagnostics::Result result = rtspClient.open(url);
    if (result.errorCode != 0)
    {
        NX_ERROR(this, "Failed to open rtsp stream: %1", result.toString(nullptr));
        failed = true;
        return;
    }
    rtspClient.play(position, AV_NOPTS_VALUE, 1.0);
    int rtpChannelNum = -1;
    std::vector<QnByteArray*> dataArrays;
    m_lastFrameTime = std::chrono::system_clock::now();
    while (true)
    {
        int bytesRead = rtspClient.readBinaryResponse(dataArrays, rtpChannelNum);
        if (rtpChannelNum >= 0 && (int)dataArrays.size() > rtpChannelNum && dataArrays[rtpChannelNum])
        {
            if (bytesRead > 0 && config.printTimestamps)
            {
                parsePacketTimestamp(
                    (uint8_t*)dataArrays[rtpChannelNum]->data() + kInterleavedRtpOverTcpPrefixLength,
                    dataArrays[rtpChannelNum]->size() - kInterleavedRtpOverTcpPrefixLength,
                    cameraId,
                    config.timeout);
            }
            dataArrays[rtpChannelNum]->clear();
        }
        if (bytesRead <= 0)
        {
            NX_ERROR(this, "Failed to read data");
            failed = true;
            break;
        }
        totalBytesRead += bytesRead;
    }
    for (auto& data: dataArrays)
        delete data;
}

void RtspPerf::Session::parsePacketTimestamp(
    const uint8_t* data, int64_t size, const QString& cameraId, std::chrono::milliseconds timeout)
{
    using namespace nx::streaming::rtp;

    static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;

    const RtpHeader* rtpHeader = (RtpHeader*) data;
    if (rtpHeader->CSRCCount != 0 || rtpHeader->version != 2)
    {
        NX_WARNING(this, "Got malformed RTP packet header. Ignored.");
        return;
    }

    const quint8* payload = data + RtpHeader::kSize;
    size -= RtpHeader::kSize;
    // Odd numbers - codec context, even numbers - data. Ignore context
    if ((qFromBigEndian(rtpHeader->ssrc) & 0x01) != 0)
        return;

    if (rtpHeader->padding)
        size -= qFromBigEndian(rtpHeader->padding);

    auto nowTime = std::chrono::system_clock::now();
    if (newPacket)
    {
        if (size < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
            return;

        const QnAbstractMediaData::DataType dataType = (QnAbstractMediaData::DataType)*(payload++);
        const quint32 timestampHigh = qFromBigEndian(*(quint32*) payload);
        const int64_t timestamp = qFromBigEndian(rtpHeader->timestamp) + (qint64(timestampHigh) << 32);
        if (dataType == QnAbstractMediaData::VIDEO)
        {
            printf("Camera %s timestamp %lld us\n", cameraId.toUtf8().data(),
                (long long int) timestamp);
            m_lastFrameTime = nowTime;
        }
    }

    std::chrono::duration<double> timeFromLastFrame = nowTime - m_lastFrameTime;
    if (timeFromLastFrame > timeout)
    {
        printf("WARNNIG: camera %s, video frame was not received for %lfsec\n",
            cameraId.toUtf8().data(), timeFromLastFrame.count());
        m_lastFrameTime = nowTime;
    }
    newPacket = rtpHeader->marker;
}
