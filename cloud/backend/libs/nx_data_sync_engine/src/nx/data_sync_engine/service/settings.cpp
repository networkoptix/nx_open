#include "settings.h"

namespace nx::clusterdb::engine {

Settings::Settings():
    base_type("", "", "") //< TODO
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

void Settings::loadSettings()
{
    m_logging.load(settings());
    m_syncSettings.load(settings());
    m_db.loadFromSettings(settings());
    m_http.load(settings());

    if (m_http.endpoints.empty() && m_http.sslEndpoints.empty())
        m_http.endpoints.push_back(nx::network::SocketAddress::anyPrivateAddressV4);
}

} // namespace nx::clusterdb::engine
