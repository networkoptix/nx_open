
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

private:
    nx::cdb::CdbLauncher m_cdb;
    MediaServerLauncher m_mediaServerLauncher;
    std::unique_ptr<MediaServerClient> m_mserverClient;
};
