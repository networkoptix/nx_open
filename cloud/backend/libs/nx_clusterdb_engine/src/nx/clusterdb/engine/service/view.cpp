#include "view.h"

#include "controller.h"
#include "settings.h"
#include "../http/http_paths.h"

namespace nx::clusterdb::engine {

View::View(
    const Settings& settings,
    Controller* controller)
    :
    m_httpServer(nx::network::http::server::Builder::build(
        settings.http(),
        nullptr, //< TODO: Authentication.
        &m_httpMessageDispatcher))
{
    controller->syncronizationEngine().registerHttpApi(
        kBaseSynchronizationPath,
        &m_httpMessageDispatcher);
}

std::vector<nx::network::SocketAddress> View::httpEndpoints() const
{
    return m_httpServer->endpoints();
}

void View::listen()
{
    m_httpServer->listen();
}

} // namespace nx::clusterdb::engine
