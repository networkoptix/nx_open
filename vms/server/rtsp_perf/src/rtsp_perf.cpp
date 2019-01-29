#include "rtsp_perf.h"

#include <thread>

#include <nx/streaming/rtsp_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/camera_data_ex.h>

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

void RtspPerf::run()
{
    static const std::chrono::milliseconds kSessionStartInterval(20);
    static const std::chrono::seconds kStatisticPrintInterval(1);
    nx::network::SocketGlobals::InitGuard socketInitializationGuard;
    std::list<std::thread> workers;
    std::vector<QString> urls;
    if (!getCamerasUrls(m_config.server, urls))
        return;

    for (int32_t i = 0; i < m_config.count; ++i)
    {
        std::this_thread::sleep_for(kSessionStartInterval);
        bool live = i < m_config.count * m_config.livePercent / 100;
        QString url = urls[i % urls.size()];
        workers.emplace_back([url, live, this]() { startSession(url, live); } );
    }

    while (true)
    {
        std::this_thread::sleep_for(kStatisticPrintInterval);
        NX_DEBUG(this, "Total bytesRead %1, success %2, failed %3",
            m_stat.totalBytesRead, m_stat.successStreams, m_stat.failedStreams);
    }

    for (auto& i: workers)
        i.join();
}

void RtspPerf::startSession(const QString& url, bool live)
{
    int64_t position = DATETIME_NOW;
    if (!live)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(60, 3600);
        position = QDateTime::currentMSecsSinceEpoch() * 1000 - dis(gen) * 1000000;
    }
    NX_DEBUG(this, "Start test rtps session: %1 %2",
        url,
        live ? "live" : "archive, from position: " + QString::number(position / 1000000));
    QAuthenticator auth;
    auth.setUser(m_config.user);
    auth.setPassword(m_config.password);
    QnRtspClient::Config config;
    QnRtspClient rtspClient(config);
    rtspClient.setAuth(auth, nx::network::http::header::AuthScheme::basic);
    rtspClient.setTransport(RtspTransport::tcp);
    rtspClient.setAdditionAttribute(Qn::EC2_INTERNAL_RTP_FORMAT, "1");
    CameraDiagnostics::Result result = rtspClient.open(url);
    if (result.errorCode != 0)
    {
        NX_ERROR(this, "Failed to open rtsp stream: %1", result.toString(nullptr));
        ++m_stat.failedStreams;
        return;
    }

    rtspClient.play(position, AV_NOPTS_VALUE, 1.0);
    int rtpChannelNum = -1;
    std::vector<QnByteArray*> demuxedData;
    bool successCounted = false;
    while (true)
    {
        int bytesRead = rtspClient.readBinaryResponce(demuxedData, rtpChannelNum);
        if (bytesRead <= 0)
        {
            NX_ERROR(this, "Failed to read data");
            ++m_stat.failedStreams;
            break;
        }
        else if (!successCounted)
        {
            successCounted = true;
            ++m_stat.successStreams;
        }
        m_stat.totalBytesRead += bytesRead;
    }
}
