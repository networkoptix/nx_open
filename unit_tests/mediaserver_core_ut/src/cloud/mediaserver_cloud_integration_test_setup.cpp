#include "mediaserver_cloud_integration_test_setup.h"

#include <gtest/gtest.h>

#include <nx/utils/string.h>

#include <cdb/connection.h>
#include <api/app_server_connection.h>
#include <utils/common/sync_call.h>

#include <media_server/settings.h>

using namespace nx::cdb;

MediaServerCloudIntegrationTest::MediaServerCloudIntegrationTest():
    m_defaultOwnerCredentials({ "admin", "admin" })
{
    init();
}

MediaServerCloudIntegrationTest::~MediaServerCloudIntegrationTest()
{
}

bool MediaServerCloudIntegrationTest::startCloudDB()
{
    return m_cdb.startAndWaitUntilStarted();
}

bool MediaServerCloudIntegrationTest::startMediaServer()
{
    m_mediaServerLauncher.addSetting(nx_ms_conf::CDB_ENDPOINT, m_cdb.endpoint().toString());

    if (!m_mediaServerLauncher.start())
        return false;

    // TODO: #ak Remove it from here when mediaserver does it from QnMain::run, not from application's main thread.
    QnCommonMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2());

    m_mserverClient = std::make_unique<MediaServerClient>(
        m_mediaServerLauncher.endpoint());
    m_mserverClient->setUserName(lit("admin"));
    m_mserverClient->setPassword(lit("admin"));
    return true;
}

bool MediaServerCloudIntegrationTest::registerRandomCloudAccount()
{
    api::AccountData accountData;
    if (m_cdb.addActivatedAccount(&accountData, &m_accountPassword) != api::ResultCode::ok)
        return false;
    m_accountEmail = accountData.email;
    return true;
}

bool MediaServerCloudIntegrationTest::registerCloudSystem()
{
    return m_cdb.bindRandomSystem(m_accountEmail, m_accountPassword, &m_cloudSystem) ==
        api::ResultCode::ok;
}

bool MediaServerCloudIntegrationTest::saveCloudCredentialsToMediaServer()
{
    CloudCredentialsData cloudData;
    cloudData.cloudSystemID = QString::fromStdString(m_cloudSystem.id);
    cloudData.cloudAuthKey = QString::fromStdString(m_cloudSystem.authKey);
    cloudData.cloudAccountName = QString::fromStdString(m_accountEmail);
    return
        m_mserverClient->saveCloudSystemCredentials(std::move(cloudData)).error ==
        QnJsonRestResult::NoError;
}

SocketAddress MediaServerCloudIntegrationTest::mediaServerEndpoint() const
{
    return m_mediaServerLauncher.endpoint();
}

nx::cdb::CdbLauncher* MediaServerCloudIntegrationTest::cdb()
{
    return &m_cdb;
}

MediaServerClient MediaServerCloudIntegrationTest::prepareMediaServerClient()
{
    MediaServerClient mediaServerClient(mediaServerEndpoint());
    if (!m_ownerCredentials.first.isEmpty())
    {
        mediaServerClient.setUserName(m_ownerCredentials.first);
        mediaServerClient.setPassword(m_ownerCredentials.second);
    }
    else
    {
        mediaServerClient.setUserName(m_defaultOwnerCredentials.first);
        mediaServerClient.setPassword(m_defaultOwnerCredentials.second);
    }
    return mediaServerClient;
}

void MediaServerCloudIntegrationTest::configureSystemAsLocal()
{
    auto mediaServerClient = prepareMediaServerClient();

    const QString password = nx::utils::generateRandomName(7);

    SetupLocalSystemData request;
    request.systemName = nx::utils::generateRandomName(7);
    request.password = password;

    QnJsonRestResult resultCode = mediaServerClient.setupLocalSystem(std::move(request));
    ASSERT_EQ(QnJsonRestResult::NoError, resultCode.error);

    if (m_ownerCredentials.first.isEmpty())
    {
        m_ownerCredentials.first = "admin";
        m_ownerCredentials.second = password;
    }
}

void MediaServerCloudIntegrationTest::connectSystemToCloud()
{
    ASSERT_TRUE(registerRandomCloudAccount());
    ASSERT_TRUE(registerCloudSystem());
    ASSERT_TRUE(saveCloudCredentialsToMediaServer());

    if (m_ownerCredentials.first.isEmpty())
    {
        m_ownerCredentials.first = m_accountEmail.c_str();
        m_ownerCredentials.second = m_accountPassword.c_str();
    }
}

void MediaServerCloudIntegrationTest::init()
{
    ASSERT_TRUE(startCloudDB());
    ASSERT_TRUE(startMediaServer());
}
