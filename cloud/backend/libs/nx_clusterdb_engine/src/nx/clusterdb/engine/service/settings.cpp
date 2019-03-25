#include "settings.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "../http/http_paths.h"

namespace nx::clusterdb::engine {

static constexpr char kApiBaseHttpPath[] = "api/baseHttpPath";

Settings::Settings(
    const QString& organizationName,
    const QString& applicationName,
    const QString& moduleName)
    :
    base_type(organizationName, applicationName, moduleName)
{
}

QString Settings::dataDir() const
{
    // TODO
    return "";
}

utils::log::Settings Settings::logging() const
{
    return m_logging;
}

const SynchronizationSettings& Settings::synchronization() const
{
    return m_syncSettings;
}

const nx::sql::ConnectionOptions& Settings::db() const
{
    return m_db;
}

const nx::network::http::server::Settings& Settings::http() const
{
    return m_http;
}

const nx::cloud::discovery::Settings& Settings::discovery() const
{
    return m_discovery;
}

const Api& Settings::api() const
{
    return m_api;
}

void Settings::loadSettings()
{
    m_logging.load(settings());
    m_syncSettings.load(settings());
    m_db.loadFromSettings(settings());
    m_http.load(settings());
    m_discovery.load(settings());
    loadApi();

    if (m_http.endpoints.empty() && m_http.sslEndpoints.empty())
        m_http.endpoints.push_back(nx::network::SocketAddress::anyPrivateAddressV4);
}

void Settings::loadApi()
{
    m_api.baseHttpPath = settings().value(kApiBaseHttpPath, kBaseSynchronizationPath)
        .toString().toStdString();
}

} // namespace nx::clusterdb::engine
