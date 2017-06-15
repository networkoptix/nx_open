#include "mediaserver_cloud_integration_test_setup.h"

#include <gtest/gtest.h>

#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/test_support/utils.h>

#include <api/app_server_connection.h>
#include <nx/cloud/cdb/api/connection.h>
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
    m_mediaServerLauncher.addSetting(nx_ms_conf::DELAY_BEFORE_SETTING_MASTER_FLAG, "100ms");

    if (!m_mediaServerLauncher.start())
        return false;

    // TODO: #ak Remove it from here when mediaserver does it from QnMain::run, not from application's main thread.
    m_mediaServerLauncher.commonModule()->messageProcessor()->init(
        m_mediaServerLauncher.commonModule()->ec2Connection());
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
    if (m_cdb.bindRandomSystem(m_accountEmail, m_accountPassword, &m_cloudSystem) !=
            api::ResultCode::ok)
    {
        return false;
    }

    if (m_cdb.getSystemSharing(
            m_accountEmail,
            m_accountPassword,
            m_cloudSystem.id,
            m_accountEmail,
            &m_systemOwnerInfo) != nx::cdb::api::ResultCode::ok)
    {
        return false;
    }

    return true;
}

bool MediaServerCloudIntegrationTest::saveCloudCredentialsToMediaServer()
{
    auto mserverClient = prepareMediaServerClient();

    CloudCredentialsData cloudData;
    cloudData.cloudSystemID = QString::fromStdString(m_cloudSystem.id);
    cloudData.cloudAuthKey = QString::fromStdString(m_cloudSystem.authKey);
    cloudData.cloudAccountName = QString::fromStdString(m_accountEmail);
    return
        mserverClient.saveCloudSystemCredentials(std::move(cloudData)).error ==
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

MediaServerLauncher& MediaServerCloudIntegrationTest::mediaServer()
{
    return m_mediaServerLauncher;
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

MediaServerClient MediaServerCloudIntegrationTest::prepareMediaServerClientFromCloudOwner()
{
    MediaServerClient mediaServerClient(mediaServerEndpoint());
    mediaServerClient.setUserName(m_accountEmail.c_str());
    mediaServerClient.setPassword(m_accountPassword.c_str());
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

void MediaServerCloudIntegrationTest::switchToDefaultCredentials()
{
    m_ownerCredentials.first.clear();
    m_ownerCredentials.second.clear();
}

void MediaServerCloudIntegrationTest::init()
{
    ASSERT_TRUE(startCloudDB());
    ASSERT_TRUE(startMediaServer());
}
