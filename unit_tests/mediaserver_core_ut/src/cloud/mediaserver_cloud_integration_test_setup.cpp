#include "mediaserver_cloud_integration_test_setup.h"

#include <gtest/gtest.h>

#include <nx/utils/string.h>

#include <cdb/connection.h>
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

    m_mserverClient = std::make_unique<MediaServerClient>(
        m_mediaServerLauncher.endpoint());
    m_mserverClient->setUserName(lit("admin"));
    m_mserverClient->setPassword(lit("admin"));
    return true;
}

bool MediaServerCloudIntegrationTest::registerRandomCloudAccount(
    std::string* const accountEmail,
    std::string* const accountPassword)
{
    api::AccountData accountData;
    if (m_cdb.addActivatedAccount(&accountData, accountPassword) != api::ResultCode::ok)
        return false;
    *accountEmail = accountData.email;
    return true;
}

bool MediaServerCloudIntegrationTest::bindSystemToCloud(
    const std::string& accountEmail,
    const std::string& accountPassword,
    std::string* const cloudSystemId,
    std::string* const cloudSystemAuthKey)
{
    api::SystemData system1;
    const auto result = m_cdb.bindRandomSystem(accountEmail, accountPassword, &system1);
    if (result != api::ResultCode::ok)
        return false;

    *cloudSystemId = system1.id;
    *cloudSystemAuthKey = system1.authKey;

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
    ASSERT_TRUE(registerRandomCloudAccount(&m_accountEmail, &m_accountPassword));
    ASSERT_TRUE(bindSystemToCloud(
        m_accountEmail, m_accountPassword,
        &m_cloudSystemId, &m_cloudSystemAuthKey));

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
