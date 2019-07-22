#pragma once

#include <nx/utils/basic_service_settings.h>

#include <nx/network/http/server/settings.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::service {

struct Http:
    public network::http::server::Settings
{
    std::string htdigestPath;

    Http(const char* groupName);
};

struct Server
{
    std::string name;
};

struct CloudDb
{
    nx::utils::Url url;
};

struct Statistics
{
    bool enabled = true;
};

class Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    const Http& http() const;
    const Server& server() const;
    const CloudDb& cloudDb() const;
    const Statistics& statistics() const;

protected:
    virtual void loadSettings() override;

private:
    void loadHttp();
    void loadServer();
    void loadCloudDb();
    void loadStatistics();

private:
    Http m_http;
    Server m_server;
    CloudDb m_cloudDb;
    Statistics m_statistics;
};

} // namespace nx::cloud::storage::service
