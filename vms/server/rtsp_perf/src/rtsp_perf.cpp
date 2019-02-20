#include "rtsp_perf.h"

#include <nx/streaming/rtsp_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/camera_data_ex.h>

static const std::chrono::milliseconds kSessionStartInterval(100);

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

void RtspPerf::startSessions(const std::vector<QString>& urls)
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
                m_sessions[i].run(url, m_config.user, m_config.password, live);
            });
            ++m_totalFailed;
            std::this_thread::sleep_for(kSessionStartInterval);
        }
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

    uint64_t lastBytesCount = 0;
    m_totalFailed = -m_config.count;
    while (true)
    {
        startSessions(urls);
        int64_t totalBytesRead = 0;
        std::this_thread::sleep_for(kStatisticPrintInterval);

        for (const auto& session: m_sessions)
            totalBytesRead += session.totalBytesRead;
        float bitrate = float(totalBytesRead - lastBytesCount) * 8 /
            std::chrono::duration_cast<std::chrono::seconds>(kStatisticPrintInterval).count() / 1000000;
        NX_INFO(this, "Total bitrate %1 MBit/s, failed %2, bytes read %3",
            QString::number(bitrate, 'f', 3), m_totalFailed, totalBytesRead);
        lastBytesCount = totalBytesRead;
    }
}

void RtspPerf::Session::run(
    const QString& url, const QString& user, const QString& password, bool live)
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
    auth.setUser(user);
    auth.setPassword(password);
    QnRtspClient::Config config;
    QnRtspClient rtspClient(config);
    rtspClient.setAuth(auth, nx::network::http::header::AuthScheme::basic);
    rtspClient.setTransport(RtspTransport::tcp);
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
    std::vector<QnByteArray*> demuxedData;
    while (true)
    {
        int bytesRead = rtspClient.readBinaryResponce(demuxedData, rtpChannelNum);
        if (rtpChannelNum >= 0 && (int)demuxedData.size() > rtpChannelNum && demuxedData[rtpChannelNum])
            demuxedData[rtpChannelNum]->clear();
        if (bytesRead <= 0)
        {
            NX_ERROR(this, "Failed to read data");
            failed = true;
            break;
        }
        totalBytesRead += bytesRead;
    }
    for (auto& data: demuxedData)
        delete data;
}
