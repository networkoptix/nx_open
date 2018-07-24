#pragma once

#include <memory>

#include <gtest/gtest.h>

#include <nx/cloud/cdb/test_support/cdb_launcher.h>

#include <common/static_common_module.h>
#include <api/mediaserver_client.h>
#include <test_support/mediaserver_launcher.h>

struct TestParams
{
    TestParams() {}
    TestParams(bool p2pMode) : p2pMode(p2pMode) {}

    bool p2pMode = false;
};

class MediaServerCloudIntegrationTest:
    public ::testing::TestWithParam<TestParams>
{
public:
    MediaServerCloudIntegrationTest();
    virtual ~MediaServerCloudIntegrationTest();

    bool startCloudDB();
    bool startMediaServer();

    bool registerRandomCloudAccount();
    bool registerCloudSystem();
    bool saveCloudCredentialsToMediaServer();

    nx::network::SocketAddress mediaServerEndpoint() const;
    nx::cdb::CdbLauncher* cdb();
    MediaServerLauncher& mediaServer();

    std::unique_ptr<MediaServerClient> prepareMediaServerClient();
    std::unique_ptr<MediaServerClient> prepareMediaServerClientFromCloudOwner();
    void configureSystemAsLocal();
    void connectSystemToCloud();

    std::string accountEmail() const { return m_ownerAccount.email; }
    std::string accountPassword() const { return m_ownerAccount.password; }
    std::string cloudOwnerVmsUserId() const { return m_systemOwnerInfo.vmsUserId; }
    nx::cdb::api::SystemData cloudSystem() const { return m_cloudSystem; }
    const nx::cdb::AccountWithPassword& ownerAccount() const { return m_ownerAccount; }

    void changeCloudOwnerAccountPassword();
    void switchToDefaultCredentials();
    void waitForCloudDataSynchronizedToTheMediaServer();
    nx::vms::api::UserData inviteRandomCloudUser();
    void waitForUserToAppearInCloud(const std::string& email);

    static void SetUpTestCase();

    static void TearDownTestCase();

protected:
    virtual void SetUp() override;

private:
    nx::cdb::CdbLauncher m_cdb;
    MediaServerLauncher m_mediaServerLauncher;
    const std::pair<QString, nx::String> m_defaultOwnerCredentials;
    std::pair<QString, nx::String> m_ownerCredentials;
    nx::cdb::api::SystemData m_cloudSystem;
    nx::cdb::api::SystemSharingEx m_systemOwnerInfo;
    nx::cdb::AccountWithPassword m_ownerAccount;

    std::string m_cloudSystemId;
    std::string m_cloudSystemAuthKey;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;

    std::unique_ptr<MediaServerClient> allocateMediaServerClient();
};
