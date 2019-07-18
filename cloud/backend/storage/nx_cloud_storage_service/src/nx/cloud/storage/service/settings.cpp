#include "settings.h"

#include <nx/utils/app_info.h>
#include <nx/utils/log/log_message.h>

namespace nx::cloud::storage::service {

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

namespace statistics {

static constexpr char kGroupName[] = "statistics";
static constexpr char kEnabled[] = "enabled";
static constexpr bool kDefaultEnabled = false;

}

Http::Http(const char* groupName):
    network::http::server::Settings(groupName)
{
}

Settings::Settings():
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
    loadStatistics();
}

void Settings::loadHttp()
{
    m_http.load(settings());
    m_http.htdigestPath = settings().value(
        lm("%1/%2").args(http::kGroupName, http::kHtdigestPath)).toString().toStdString();
}

void Settings::loadServer()
{
    m_server.name = settings().value(
        lm("%1/%2").args(server::kGroupName, server::kName)).toString().toStdString();
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

const Statistics& Settings::statistics() const
{
    return m_statistics;
}

} // namespace nx::cloud::storage::service
