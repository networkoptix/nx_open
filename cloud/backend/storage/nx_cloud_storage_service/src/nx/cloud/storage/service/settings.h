#pragma once

#include <nx/utils/basic_service_settings.h>

#include <nx/cloud/aws/credentials.h>
#include <nx/clusterdb/engine/p2p_sync_settings.h>
#include <nx/network/http/server/settings.h>
#include <nx/sql/types.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::service::conf {

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

struct Aws
{
    nx::cloud::aws::Credentials credentials;
    nx::utils::Url stsUrl;
    std::string assumeRoleArn;
    // Corresponds to aws default value
    std::chrono::seconds storageCredentialsDuration;
};

struct GeoIp
{
    std::string dbPath;
};

struct Database
{
    nx::sql::ConnectionOptions sql;
    nx::clusterdb::engine::SynchronizationSettings synchronization;
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
    const Aws& aws() const;
    const GeoIp& geoIp() const;
    const Database& database() const;
    const Statistics& statistics() const;

protected:
    virtual void loadSettings() override;

private:
    void loadHttp();
    void loadServer();
    void loadCloudDb();
    void loadAws();
    void loadGeoIp();
    void loadDatabase();
    void loadStatistics();

private:
    Http m_http;
    Server m_server;
    CloudDb m_cloudDb;
    Aws m_aws;
    GeoIp m_geoIp;
    Database m_database;
    Statistics m_statistics;
};

} // namespace nx::cloud::storage::service::conf
