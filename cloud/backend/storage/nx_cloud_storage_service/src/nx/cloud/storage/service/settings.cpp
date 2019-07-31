#include "settings.h"

#include <nx/utils/app_info.h>
#include <nx/utils/log/log_message.h>

namespace nx::cloud::storage::service {

namespace conf {

namespace {

static constexpr char kModuleName[] = "nx_cloud_storage_service";

} // namespace

namespace http {

static constexpr char kGroupName[] = "http";
static constexpr char kHtdigestPath[] = "htdigestPath";

} // namespace http

namespace server {

static constexpr char kGroupName[] = "server";
static constexpr char kName[] = "name";

} // namespace server

namespace cloud_db {

static constexpr char kGroupName[] = "cloud_db";
static constexpr char kUrl[] = "url";
static constexpr char kDefaultUrl[] = "";

} // namespace cloud_db

namespace aws {

static constexpr char kGroupName[] = "aws";
static constexpr char kUserName[] = "userName";
static constexpr char kAuthToken[] = "authToken";

} // namespace aws

namespace geo_ip {

static constexpr char kGroupName[] = "geoIp";
static constexpr char kDbPath[] = "dbPath";

} // namespace aws

namespace database {

static constexpr char kGroupName[] = "database";
static constexpr char kSqlGroupName[] = "sql";
static constexpr char kSynchronizationGroupName[] = "p2pDb";

}

namespace statistics {

static constexpr char kGroupName[] = "statistics";
static constexpr char kEnabled[] = "enabled";
static constexpr bool kDefaultEnabled = false;

} // namespace statistics

Http::Http(const char* groupName):
    network::http::server::Settings(groupName)
{
}

Settings::Settings() :
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(),
        kModuleName,
        kModuleName),
    m_http(http::kGroupName)
{
}

void Settings::loadSettings()
{
    loadHttp();
    loadServer();
    loadCloudDb();
    loadAws();
    loadGeoIp();
    loadDatabase();
    loadStatistics();
}

void Settings::loadHttp()
{
    using namespace http;
    m_http.load(settings());
    m_http.htdigestPath = settings().value(
        lm("%1/%2").args(kGroupName, kHtdigestPath)).toString().toStdString();
}

void Settings::loadServer()
{
    using namespace server;
    m_server.name = settings().value(
        lm("%1/%2").args(kGroupName, kName)).toString().toStdString();
}

void Settings::loadCloudDb()
{
    using namespace cloud_db;
    m_cloudDb.url = settings().value(lm("%1/%2").args(kGroupName, kUrl), kDefaultUrl).toString();
}

void Settings::loadAws()
{
    using namespace aws;

    QString accessKeyId = settings().value(
        lm("%1/%2").args(kGroupName, kUserName)).toString();
    network::http::Ha1AuthToken secretAccessKey(
        settings().value(lm("%1/%2").args(kGroupName, kAuthToken)).toByteArray());

    m_aws.credentials = network::http::Credentials(accessKeyId, secretAccessKey);
}

void Settings::loadGeoIp()
{
    using namespace geo_ip;

    m_geoIp.dbPath = settings().value(
        lm("%1/%2").args(kGroupName, kDbPath)).toString().toStdString();
}

void Settings::loadDatabase()
{
    using namespace database;
    m_database.sql.loadFromSettings(
        settings(),
        lm("%1/%2").args(kGroupName, kSqlGroupName));
    m_database.synchronization.load(
        settings(),
        lm("%1/%2").args(kGroupName, kSynchronizationGroupName).toStdString());
}

void Settings::loadStatistics()
{
    m_statistics.enabled = settings().value(
        lm("%1/%2").args(statistics::kGroupName, statistics::kEnabled),
        statistics::kDefaultEnabled).toBool();
}

const Http& Settings::http() const
{
    return m_http;
}

const Server& Settings::server() const
{
    return m_server;
}

const CloudDb& Settings::cloudDb() const
{
    return m_cloudDb;
}

const Aws& Settings::aws() const
{
    return m_aws;
}

const GeoIp& Settings::geoIp() const
{
    return m_geoIp;
}

const Database& Settings::database() const
{
    return m_database;
}

const Statistics& Settings::statistics() const
{
    return m_statistics;
}

} // namespace conf
} // namespace nx::cloud::storage::service
