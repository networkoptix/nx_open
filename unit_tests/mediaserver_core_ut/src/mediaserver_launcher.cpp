
#include "mediaserver_launcher.h"

#include <nx/network/http/httpclient.h>
#include <nx/utils/random.h>

#include <media_server_process.h>
#include <nx/network/socket_global.h>


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
    nx::network::SocketGlobalsHolder::instance()->reinitialize(false);

    QByteArray configFileOption = "--conf-file=" + m_configFilePath.toUtf8();
    char* argv[] = { "", "-e", configFileOption.data() };
    const int argc = 3;

    m_configFile.flush();
    m_configFile.close();

    m_mediaServerProcess.reset(new MediaServerProcess(argc, argv));

    std::promise<bool> processStartedPromise;
    auto future = processStartedPromise.get_future();

    connect(
        m_mediaServerProcess.get(),
        &MediaServerProcess::started,
        this,
        [&processStartedPromise]()
        {
            processStartedPromise.set_value(true);
        }, Qt::DirectConnection);
    m_mediaServerProcess->start();

    //waiting for server to come up
    const auto startTime = std::chrono::steady_clock::now();
    constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
    auto result = future.wait_for(maxPeriodToWaitForMediaServerStart);
    return result == std::future_status::ready;
}

bool MediaServerLauncher::stop()
{
    m_mediaServerProcess->stopSync();
    return true;
}
