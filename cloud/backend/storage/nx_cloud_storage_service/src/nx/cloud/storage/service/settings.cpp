#include "settings.h"

#include <nx/utils/app_info.h>
#include <nx/utils/timer_manager.h>

namespace nx::cloud::storage::service {

namespace conf {

namespace {

static constexpr char kModuleName[] = "storage_service";

} // namespace

namespace http {

static constexpr char kGroupName[] = "http";
static constexpr char kHtdigestPath[] = "htdigestPath";
static constexpr char kDefaultEndpoint[] = "0.0.0.0:3375";
static constexpr char kDefaultSslEndpoint[] = "0.0.0.0:3385";

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
static constexpr char kStsUrl[] = "stsUrl";
static constexpr char kDefaultStsUrl[] = "https://sts.amazonaws.com";
static constexpr char kAccessKeyId[] = "accessKeyId";
static constexpr char kSecretAccessKey[] = "secretAccessKey";
static constexpr char kAssumeRoleArn[] = "assumeRoleArn";
static constexpr char kStorageCredentialsDuration[] = "storageCredentialsDuration";
static constexpr std::chrono::seconds kDefaultStorageCredentialsDuration =
    std::chrono::seconds(3600);

} // namespace aws

namespace geo_ip {

static constexpr char kGroupName[] = "geoIp";
static constexpr char kDbPath[] = "dbPath";

} // namespace aws

namespace database {

static constexpr char kGroupName[] = "database";
static constexpr char kSqlGroupName[] = "sql";
static constexpr char kSynchronizationGroupName[] = "p2pDb";

} //namespace database

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

    if (m_http.endpoints.empty())
        m_http.endpoints.emplace_back(kDefaultEndpoint);

    if (m_http.sslEndpoints.empty())
        m_http.sslEndpoints.emplace_back(kDefaultSslEndpoint);
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
    using namespace std::chrono;

    m_aws.credentials.username =
        settings().value(lm("%1/%2").args(kGroupName, kAccessKeyId)).toString();
    m_aws.credentials.authToken = network::http::Ha1AuthToken(
        settings().value(lm("%1/%2").args(kGroupName, kSecretAccessKey)).toByteArray());

    m_aws.stsUrl = settings().value(
        lm("%1/%2").args(kGroupName, kStsUrl), kDefaultStsUrl).toString().toStdString();

    m_aws.assumeRoleArn = settings().value(
        lm("%1/%2").args(kGroupName, kAssumeRoleArn)).toString().toStdString();

    m_aws.storageCredentialsDuration = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            settings().value(lm("%1/%2").args(kGroupName, kStorageCredentialsDuration))
                .toString(),
            kDefaultStorageCredentialsDuration));
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
    using namespace statistics;
    m_statistics.enabled = settings().value(
        lm("%1/%2").args(kGroupName, kEnabled),
        kDefaultEnabled).toBool();
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
