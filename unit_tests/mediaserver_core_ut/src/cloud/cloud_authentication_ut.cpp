
#include <fstream>

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/network/http/httpclient.h>
#include <nx/utils/literal.h>
#include <nx/utils/random.h>
#include <utils/common/sync_call.h>
#include <utils/common/util.h>

#include <media_server/media_server_module.h>
#include <media_server_process.h>
#include <platform/platform_abstraction.h>
#include <test_support/cdb_launcher.h>
#include <utils/common/long_runnable.h>

#include "media_server/serverutil.h"
#include "mediaserver_client.h"
#include "utils.h"


class MediaServerLauncher
{
public:
    MediaServerLauncher(const QString& tmpDir = QString())
    :
        m_workDirResource(tmpDir),
        m_serverEndpoint(HostAddress::localhost, nx::utils::random::number<int>(45000, 50000))
        //m_platform(new QnPlatformAbstraction()),
        //m_runnablePool(new QnLongRunnablePool()),
        //m_module(new QnMediaServerModule())
    {
        //ASSERT_TRUE((bool)m_workDirResource.getDirName());

        m_configFilePath = *m_workDirResource.getDirName() + lit("/mserver.conf");

        m_configFile.open(m_configFilePath.toUtf8().constData());
        //ASSERT_TRUE(m_configFile.is_open());

        m_configFile<<"serverGuid = "<< QnUuid::createUuid().toString().toStdString()<<std::endl;
        m_configFile<<"removeDbOnStartup = 1"<<std::endl;
        m_configFile<<"dataDir = "<< m_workDirResource.getDirName()->toStdString()<<std::endl;
        m_configFile<<"varDir = "<< m_workDirResource.getDirName()->toStdString()<<std::endl;
        //m_configFile<<"eventsDBFilePath = "<< closeDirPath(getDataDirectory()).toStdString()<<std::endl;
        m_configFile<<"systemName = "<< QnUuid::createUuid().toString().toStdString()<<std::endl;
        m_configFile<<"port = "<< m_serverEndpoint.port<<std::endl;
    }

    ~MediaServerLauncher()
    {
        //if (m_mserverProcess)
            stop();
    }

    SocketAddress endpoint() const
    {
        return m_serverEndpoint;
    }

    void addSetting(const QString& name, const QString& value)
    {
        m_configFile<<name.toStdString()<<" = "<<value.toStdString()<<std::endl;
    }

    bool start()
    {
        QByteArray configFileOption = "--conf-file=" + m_configFilePath.toUtf8();
        char* argv[] = { "", "-e", configFileOption.data() };
        const int argc = 3;

        //m_mserverProcess = std::make_unique<MediaServerProcess>(argc, argv);
        m_configFile.flush();
        m_configFile.close();
        m_mediaServerProcessThread = std::thread(
            [this, argc, argv = argv]() mutable
            {
                MediaServerProcess::main(argc, argv);
                //m_mserverProcess->run();
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

    bool stop()
    {
        nx_http::HttpClient httpClient;
        httpClient.setUserName(lit("admin"));
        httpClient.setUserPassword(lit("admin"));
        if (!httpClient.doGet(lit("http://%1/api/restart").arg(m_serverEndpoint.toString())))
            return false;
        //m_mserverProcess->pleaseStop();
        m_mediaServerProcessThread.join();
        return true;
    }

private:
    std::ofstream m_configFile;
    nx::ut::utils::WorkDirResource m_workDirResource;
    std::unique_ptr<MediaServerProcess> m_mserverProcess;
    SocketAddress m_serverEndpoint;
    QString m_configFilePath;
    std::thread m_mediaServerProcessThread;

    QScopedPointer<QnPlatformAbstraction> m_platform;
    QScopedPointer<QnLongRunnablePool> m_runnablePool;
    QScopedPointer<QnMediaServerModule> m_module;
};

using namespace nx::cdb;

class MediaServerCloudTest
{
public:
    MediaServerCloudTest()
    {
    }

    virtual ~MediaServerCloudTest()
    {
    }

    MediaServerLauncher* mediaServerLauncher()
    {
    }

    bool startCloudDB()
    {
        return m_cdb.startAndWaitUntilStarted();
    }

    bool startMediaServer()
    {
        m_mediaServerLauncher.addSetting(lit("cdbEndpoint"), m_cdb.endpoint().toString());

        if (!m_mediaServerLauncher.start())
            return false;

        m_mserverClient = std::make_unique<MediaServerClient>(
            m_mediaServerLauncher.endpoint());
        m_mserverClient->setUserName(lit("admin"));
        m_mserverClient->setPassword(lit("admin"));
        return true;
    }

    bool registerRandomCloudAccount(
        std::string* const accountEmail,
        std::string* const accountPassword)
    {
        api::AccountData accountData;
        if (m_cdb.addActivatedAccount(&accountData, accountPassword) != api::ResultCode::ok)
            return false;
        *accountEmail = accountData.email;
        return true;
    }

    bool bindSystemToCloud(
        const std::string& accountEmail,
        const std::string& accountPassword)
    {
        api::SystemData system1;
        const auto result = m_cdb.bindRandomSystem(accountEmail, accountPassword, &system1);
        if (result != api::ResultCode::ok)
            return false;

        CloudCredentialsData cloudData;
        cloudData.cloudSystemID = QString::fromStdString(system1.id);
        cloudData.cloudAuthKey = QString::fromStdString(system1.authKey);
        cloudData.cloudAccountName = QString::fromStdString(accountEmail);
        QnJsonRestResult resultCode;
        std::tie(resultCode) =
            makeSyncCall<QnJsonRestResult>(
                std::bind(
                    &MediaServerClient::saveCloudSystemCredentials,
                    m_mserverClient.get(),
                    std::move(cloudData),
                    std::placeholders::_1));
        return resultCode.error == QnJsonRestResult::NoError;
    }

    SocketAddress mediaServerEndpoint() const
    {
        return m_mediaServerLauncher.endpoint();
    }

private:
    nx::cdb::CdbLauncher m_cdb;
    MediaServerLauncher m_mediaServerLauncher;
    std::unique_ptr<MediaServerClient> m_mserverClient;
};

class CloudAuthentication
:
    public MediaServerCloudTest,
    public ::testing::Test
{
};

#ifdef ENABLE_CLOUD_TEST
TEST_F(CloudAuthentication, general)
#else
TEST_F(CloudAuthentication, DISABLED_general)
#endif
{
    ASSERT_TRUE(startCloudDB());
    ASSERT_TRUE(startMediaServer());

    std::string accountEmail;
    std::string accountPassword;
    ASSERT_TRUE(registerRandomCloudAccount(&accountEmail, &accountPassword));
    ASSERT_TRUE(bindSystemToCloud(accountEmail, accountPassword));

    //const auto cdbConnection = getCdbConnection();

    nx_http::HttpClient httpClient;
    httpClient.setUserName(QString::fromStdString(accountEmail));
    httpClient.setUserPassword(QString::fromStdString(accountPassword));
    ASSERT_TRUE(httpClient.doGet(
        lit("http://%1/ec2/getSettings").arg(mediaServerEndpoint().toString())));
    ASSERT_NE(nullptr, httpClient.response());
    ASSERT_EQ(nx_http::StatusCode::ok, httpClient.response()->statusLine.statusCode);
}
