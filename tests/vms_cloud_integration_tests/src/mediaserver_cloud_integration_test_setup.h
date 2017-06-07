
#pragma once

#include <test_support/cdb_launcher.h>

#include "mediaserver_client.h"
#include <test_support/mediaserver_launcher.h>


class MediaServerCloudIntegrationTest
{
public:
    MediaServerCloudIntegrationTest();
    virtual ~MediaServerCloudIntegrationTest();

    bool startCloudDB();
    bool startMediaServer();

    bool registerRandomCloudAccount();
    bool registerCloudSystem();
    bool saveCloudCredentialsToMediaServer();

    SocketAddress mediaServerEndpoint() const;
    nx::cdb::CdbLauncher* cdb();

    MediaServerClient prepareMediaServerClient();
    void configureSystemAsLocal();
    void connectSystemToCloud();

    std::string accountEmail() const { return m_accountEmail; }
    std::string accountPassword() const { return m_accountPassword; }
    nx::cdb::api::SystemData cloudSystem() const { return m_cloudSystem; }

private:
    nx::cdb::CdbLauncher m_cdb;
    MediaServerLauncher m_mediaServerLauncher;
    std::unique_ptr<MediaServerClient> m_mserverClient;
    const std::pair<QString, QString> m_defaultOwnerCredentials;
    std::pair<QString, QString> m_ownerCredentials;
    nx::cdb::api::SystemData m_cloudSystem;

    std::string m_accountEmail;
    std::string m_accountPassword;
    std::string m_cloudSystemId;
    std::string m_cloudSystemAuthKey;

    void init();
};
