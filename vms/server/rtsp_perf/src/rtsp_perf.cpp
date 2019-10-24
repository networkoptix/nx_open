#include "rtsp_perf.h"

#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
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

RtspPerf::RtspPerf(const Config& config):
    m_sessions(config.count),
    m_config(config)
{
}

bool RtspPerf::getCamerasUrls(const QString& server, std::vector<QString>& urls)
{
    std::vector<QString> result;
    nx::network::http::HttpClient httpClient;
    httpClient.setUserName(m_config.sessionConfig.user);
    httpClient.setUserPassword(m_config.sessionConfig.password);

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
            QString(m_config.useSsl ? "rtsps://" : "rtsp://") + server + "/" + camera.id.toString() + "?stream=0");
    }
    return true;
}

void RtspPerf::startSessionsThread(const std::vector<QString>& urls)
{
    while (true)
    {
        for (int i = 0; i < (int) m_sessions.size(); ++i)
        {
            if (!m_sessions[i].failed)
                continue;

            bool live = i < m_config.count * m_config.livePercent / 100;
            QString url = urls[i % urls.size()];
            m_sessions[i].failed = false;
            if (m_sessions[i].worker.joinable())
                m_sessions[i].worker.join();
            m_sessions[i].worker = std::thread([url, live, this, i]()
            {
                m_sessions[i].run(url, m_config.sessionConfig, live);
            });

            if (m_sessions[i].failedCount > 0 && m_config.startInterval == std::chrono::milliseconds::zero())
                m_currentStartInterval = std::min(m_currentStartInterval * 2, kMaxStartInterval);

            std::this_thread::sleep_for(m_currentStartInterval);
        }
        if (m_config.disableRestart)
            return;
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

    m_currentStartInterval = m_config.startInterval != std::chrono::milliseconds::zero() ?
        m_config.startInterval : kMinStartInterval;
    BitrateCounter bitrateCounter;
    auto startSessionThread = std::thread([urls, this]() { startSessionsThread(urls); });
    while (true)
    {
        std::this_thread::sleep_for(kStatisticPrintInterval);
        int64_t totalBytesRead = 0;
        int64_t successSessions = 0;
        int64_t failedSessions = 0;
        for (const auto& session: m_sessions)
        {
            totalBytesRead += session.totalBytesRead;
            if (!session.failed)
                ++successSessions;
            failedSessions += session.failedCount;
        }
        float bitrate = bitrateCounter.update(totalBytesRead);
        NX_INFO(this, "Total bitrate %1 MBit/s, working sessions %2, failed %3, bytes read %4",
            QString::number(bitrate, 'f', 3), successSessions, failedSessions, totalBytesRead);
    }
}
