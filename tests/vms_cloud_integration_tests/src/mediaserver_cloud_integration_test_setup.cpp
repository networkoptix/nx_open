#include "mediaserver_cloud_integration_test_setup.h"

#include <gtest/gtest.h>

#include <nx/cloud/db/api/connection.h>
#include <nx/cloud/db/test_support/business_data_generator.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/test_support/utils.h>

#include <api/app_server_connection.h>
#include <media_server/settings.h>

using nx::cloud::db::api::ResultCode;
using nx::vms::api::UserData;
using nx::vms::api::UserDataList;

MediaServerCloudIntegrationTest::MediaServerCloudIntegrationTest():
    m_defaultOwnerCredentials({ "admin", "admin" })
{
    m_mediaServerLauncher.addSetting("delayBeforeSettingMasterFlag", "100ms");
    m_mediaServerLauncher.addSetting("p2pMode", GetParam().p2pMode);
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
    m_mediaServerLauncher.addSetting("cdbEndpoint", m_cdb.endpoint().toString());

    if (!m_mediaServerLauncher.start())
        return false;

    // TODO: #ak Remove it from here when mediaserver does it from QnMain::run, not from application's main thread.
    m_mediaServerLauncher.commonModule()->messageProcessor()->init(
        m_mediaServerLauncher.commonModule()->ec2Connection());
    return true;
}

bool MediaServerCloudIntegrationTest::registerRandomCloudAccount()
{
    if (m_cdb.addActivatedAccount(&m_ownerAccount, &m_ownerAccount.password) != ResultCode::ok)
    {
        return false;
    }
    return true;
}

bool MediaServerCloudIntegrationTest::registerCloudSystem()
{
    if (m_cdb.bindRandomSystem(m_ownerAccount.email, m_ownerAccount.password, &m_cloudSystem)
        != ResultCode::ok)
    {
        return false;
    }

    if (m_cdb.getSystemSharing(
            m_ownerAccount.email,
            m_ownerAccount.password,
            m_cloudSystem.id,
            m_ownerAccount.email,
            &m_systemOwnerInfo) != ResultCode::ok)
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
    cloudData.cloudAccountName = QString::fromStdString(m_ownerAccount.email);
    return
        mserverClient->saveCloudSystemCredentials(std::move(cloudData)).error ==
        QnJsonRestResult::NoError;
}

nx::network::SocketAddress MediaServerCloudIntegrationTest::mediaServerEndpoint() const
{
    return m_mediaServerLauncher.endpoint();
}

nx::cloud::db::CdbLauncher* MediaServerCloudIntegrationTest::cdb()
{
    return &m_cdb;
}

MediaServerLauncher& MediaServerCloudIntegrationTest::mediaServer()
{
    return m_mediaServerLauncher;
}

std::unique_ptr<MediaServerClient> MediaServerCloudIntegrationTest::prepareMediaServerClient()
{
    auto mediaServerClient = allocateMediaServerClient();
    if (!m_ownerCredentials.first.isEmpty())
    {
        mediaServerClient->setUserCredentials(nx::network::http::Credentials(
            m_ownerCredentials.first,
            nx::network::http::PasswordAuthToken(m_ownerCredentials.second)));
    }
    else
    {
        mediaServerClient->setUserCredentials(nx::network::http::Credentials(
            m_defaultOwnerCredentials.first,
            nx::network::http::PasswordAuthToken(m_defaultOwnerCredentials.second)));
    }
    return mediaServerClient;
}

std::unique_ptr<MediaServerClient>
    MediaServerCloudIntegrationTest::prepareMediaServerClientFromCloudOwner()
{
    auto mediaServerClient = allocateMediaServerClient();
    mediaServerClient->setUserCredentials(nx::network::http::Credentials(
        m_ownerAccount.email.c_str(),
        nx::network::http::PasswordAuthToken(m_ownerAccount.password.c_str())));
    return mediaServerClient;
}

void MediaServerCloudIntegrationTest::configureSystemAsLocal()
{
    auto mediaServerClient = prepareMediaServerClient();

    const auto password = nx::utils::generateRandomName(7);

    SetupLocalSystemData request;
    request.systemName = nx::utils::generateRandomName(7);
    request.password = password;

    QnJsonRestResult resultCode = mediaServerClient->setupLocalSystem(std::move(request));
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
        m_ownerCredentials.first = m_ownerAccount.email.c_str();
        m_ownerCredentials.second = m_ownerAccount.password.c_str();
    }
}

void MediaServerCloudIntegrationTest::changeCloudOwnerAccountPassword()
{
    const auto newPassword = nx::utils::generateRandomName(7).toStdString();

    nx::cloud::db::api::AccountUpdateData update;
    update.passwordHa1 = nx::network::http::calcHa1(
        m_ownerAccount.email.c_str(),
        nx::network::AppInfo::realm().toStdString().c_str(),
        newPassword.c_str()).constData();
    ASSERT_EQ(
        ResultCode::ok,
        cdb()->updateAccount(m_ownerAccount.email, m_ownerAccount.password, update));

    m_ownerAccount.password = newPassword;
    if (m_ownerCredentials.first.toStdString() == m_ownerAccount.email)
        m_ownerCredentials.second = m_ownerAccount.password.c_str();
}

void MediaServerCloudIntegrationTest::switchToDefaultCredentials()
{
    m_ownerCredentials.first.clear();
    m_ownerCredentials.second.clear();
}

void MediaServerCloudIntegrationTest::waitForCloudDataSynchronizedToTheMediaServer()
{
    const auto newAccount = cdb()->addActivatedAccount2();
    cdb()->shareSystemEx(
        m_ownerAccount,
        m_cloudSystem,
        newAccount.email,
        nx::cloud::db::api::SystemAccessRole::viewer);

    auto mediaServerClient = prepareMediaServerClient();

    for (;;)
    {
        UserDataList users;
        ASSERT_EQ(::ec2::ErrorCode::ok, mediaServerClient->ec2GetUsers(&users));
        const auto userIter = std::find_if(
            users.begin(), users.end(),
            [&newAccount](const UserData& elem)
            {
                return elem.name.toStdString() == newAccount.email;
            });
        if (userIter != users.end())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

UserData MediaServerCloudIntegrationTest::inviteRandomCloudUser()
{
    const auto userEmail =
        nx::cloud::db::test::BusinessDataGenerator::generateRandomEmailAddress();
    UserData userData;
    userData.id = guidFromArbitraryData(userEmail);
    userData.typeId = QnUuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");
    userData.isCloud = true;
    userData.isEnabled = true;
    userData.email = QString::fromStdString(userEmail);
    userData.name = QString::fromStdString(userEmail);
    //userData.userRoleId = QnUuid::createUuid();
    userData.realm = nx::network::AppInfo::realm();
    userData.hash = UserData::kCloudPasswordStub;
    userData.digest = UserData::kCloudPasswordStub;
    userData.permissions = GlobalPermission::liveViewerPermissions;

    auto mediaServerClient = prepareMediaServerClient();
    NX_GTEST_ASSERT_EQ(::ec2::ErrorCode::ok, mediaServerClient->ec2SaveUser(userData));

    return userData;
}

void MediaServerCloudIntegrationTest::waitForUserToAppearInCloud(const std::string& email)
{
    using namespace nx::cloud::db::api;

    for (;;)
    {
        std::vector<SystemSharingEx> sharings;
        ASSERT_EQ(ResultCode::ok, cdb()->getSystemSharings(
            m_ownerAccount.email, m_ownerAccount.password,
            m_cloudSystem.id, &sharings));
        auto accountIter = std::find_if(
            sharings.begin(), sharings.end(),
            [&email](const SystemSharingEx& sharing) { return sharing.accountEmail == email; });
        if (accountIter != sharings.end())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::unique_ptr<QnStaticCommonModule> MediaServerCloudIntegrationTest::s_staticCommonModule;

void MediaServerCloudIntegrationTest::SetUpTestCase()
{
    s_staticCommonModule =
        std::make_unique<QnStaticCommonModule>(nx::vms::api::PeerType::server);
}

void MediaServerCloudIntegrationTest::TearDownTestCase()
{
    s_staticCommonModule.reset();
}

void MediaServerCloudIntegrationTest::SetUp()
{
    ASSERT_TRUE(startCloudDB());
    ASSERT_TRUE(startMediaServer());
}

std::unique_ptr<MediaServerClient> MediaServerCloudIntegrationTest::allocateMediaServerClient()
{
    constexpr std::chrono::hours kRequestTimeout = std::chrono::hours(1);

    auto mediaServerClient = std::make_unique<MediaServerClient>(
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(mediaServerEndpoint()));
    mediaServerClient->setRequestTimeout(kRequestTimeout);
    return mediaServerClient;
}
