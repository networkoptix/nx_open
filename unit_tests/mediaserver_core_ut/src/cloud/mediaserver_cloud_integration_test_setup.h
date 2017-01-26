
#pragma once

#include <test_support/cdb_launcher.h>

#include "mediaserver_client.h"
#include "mediaserver_launcher.h"


class MediaServerCloudIntegrationTest
{
public:
    MediaServerCloudIntegrationTest();
    virtual ~MediaServerCloudIntegrationTest();

    bool startCloudDB();
    bool startMediaServer();

    bool registerRandomCloudAccount(
        std::string* const accountEmail,
        std::string* const accountPassword);
    bool bindSystemToCloud(
        const std::string& accountEmail,
        const std::string& accountPassword,
        std::string* const cloudSystemId,
        std::string* const cloudSystemAuthKey);

    SocketAddress mediaServerEndpoint() const;
    nx::cdb::CdbLauncher* cdb();

    MediaServerClient prepareMediaServerClient();
    void configureSystemAsLocal();
    void connectSystemToCloud();

    std::string accountEmail() const { return m_accountEmail; }
    std::string accountPassword() const { return m_accountPassword; }
    std::string cloudSystemId() const { return m_cloudSystemId; }
    std::string cloudSystemAuthKey() const { return m_cloudSystemAuthKey; }

private:
    nx::cdb::CdbLauncher m_cdb;
    MediaServerLauncher m_mediaServerLauncher;
    std::unique_ptr<MediaServerClient> m_mserverClient;
    const std::pair<QString, QString> m_defaultOwnerCredentials;
    std::pair<QString, QString> m_ownerCredentials;

    std::string m_accountEmail;
    std::string m_accountPassword;
    std::string m_cloudSystemId;
    std::string m_cloudSystemAuthKey;

    void init();
};
