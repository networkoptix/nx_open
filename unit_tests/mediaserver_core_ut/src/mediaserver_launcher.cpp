
#include "mediaserver_launcher.h"

#include <nx/network/http/httpclient.h>
#include <nx/utils/random.h>

#include <media_server_process.h>


MediaServerLauncher::MediaServerLauncher(const QString& tmpDir)
    :
    m_workDirResource(tmpDir),
    m_serverEndpoint(HostAddress::localhost, nx::utils::random::number<int>(45000, 50000))
{
    m_configFilePath = *m_workDirResource.getDirName() + lit("/mserver.conf");
    m_configFile.open(m_configFilePath.toUtf8().constData());

    m_configFile << "serverGuid = " << QnUuid::createUuid().toString().toStdString() << std::endl;
    m_configFile << "removeDbOnStartup = 1" << std::endl;
    m_configFile << "dataDir = " << m_workDirResource.getDirName()->toStdString() << std::endl;
    m_configFile << "varDir = " << m_workDirResource.getDirName()->toStdString() << std::endl;
    m_configFile << "systemName = " << QnUuid::createUuid().toString().toStdString() << std::endl;
    m_configFile << "port = " << m_serverEndpoint.port << std::endl;
}

MediaServerLauncher::~MediaServerLauncher()
{
    stop();
}

SocketAddress MediaServerLauncher::endpoint() const
{
    return m_serverEndpoint;
}

void MediaServerLauncher::addSetting(const QString& name, const QString& value)
{
    m_configFile << name.toStdString() << " = " << value.toStdString() << std::endl;
}

bool MediaServerLauncher::start()
{
    QByteArray configFileOption = "--conf-file=" + m_configFilePath.toUtf8();
    char* argv[] = { "", "-e", configFileOption.data() };
    const int argc = 3;

    m_configFile.flush();
    m_configFile.close();
    m_mediaServerProcessThread = nx::utils::thread(
        [this, argc, argv = argv]() mutable
        {
            MediaServerProcess::main(argc, argv);
        });

    //waiting for server to come up
    const auto startTime = std::chrono::steady_clock::now();
    constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
    while (std::chrono::steady_clock::now() - startTime < maxPeriodToWaitForMediaServerStart)
    {
        nx_http::HttpClient httpClient;
        if (httpClient.doGet(
            lit("http://%1/api/moduleInformation").arg(m_serverEndpoint.toString())))
            break;  //server is alive
    }

    return std::chrono::steady_clock::now() - startTime < maxPeriodToWaitForMediaServerStart;
}

bool MediaServerLauncher::stop()
{
    nx_http::HttpClient httpClient;
    httpClient.setUserName(lit("admin"));
    httpClient.setUserPassword(lit("admin"));
    if (!httpClient.doGet(lit("http://%1/api/restart").arg(m_serverEndpoint.toString())))
        return false;
    m_mediaServerProcessThread.join();
    return true;
}
