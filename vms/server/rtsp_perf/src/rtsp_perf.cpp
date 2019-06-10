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

QString RtspPerf::Config::toString()
{
    return
        "\nserver: " + server +
        "\nuser: " + user +
        "\npassword: " + password +
        "\nuse ssl: " + QString::number(useSsl) +
        "\ncount: " + QString::number(count) +
        "\nlive percent: " + QString::number(livePercent) + "%" +
        "\ntimeout: " + QString::number(timeout.count()) + "ms" +
        "\nstart interval: " + (startInterval == std::chrono::milliseconds::zero() ?
            "auto" : QString::number(startInterval.count()) + "ms") +
        "\n";
}

bool RtspPerf::getCamerasUrls(const QString& server, std::vector<QString>& urls)
{
    std::vector<QString> result;
    nx::network::http::HttpClient httpClient;
    httpClient.setUserName(m_config.user);
    httpClient.setUserPassword(m_config.password);

    if (!httpClient.doGet("http://" + server + "/ec2/getCamerasEx"))
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
                {
                    m_currentStartInterval = std::min(m_currentStartInterval * 2, kMaxStartInterval);
                }
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
    if (!getCamerasUrls(m_config.server, urls))
        return;

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
        NX_ALWAYS(this, "Total bitrate %1 MBit/s, worked sessions %2, failed %3, bytes read %4",
            QString::number(bitrate, 'f', 3), successSessions, std::max<int64_t>(m_totalFailed, 0), totalBytesRead);
    }
}

void RtspPerf::Session::run(const QString& url, const Config& config, bool live)
{
    int64_t position = DATETIME_NOW;
    if (!live)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(60, 3600);
        position = QDateTime::currentMSecsSinceEpoch() * 1000 - dis(gen) * 1000000;
    }
    NX_INFO(this, "Start test rtps session: %1 %2",
        url,
        live ? "live" : "archive, from position: " + QString::number(position / 1000000));
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
    while (true)
    {
        int bytesRead = rtspClient.readBinaryResponce(dataArrays, rtpChannelNum);
        if (rtpChannelNum >= 0 && (int)dataArrays.size() > rtpChannelNum && dataArrays[rtpChannelNum])
            dataArrays[rtpChannelNum]->clear();
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
