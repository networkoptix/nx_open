#include "view.h"

#include "controller.h"
#include "settings.h"

namespace nx::data_sync_engine {

static constexpr char kHttpPathPrefix[] = "/ec2";

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
        kHttpPathPrefix,
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

} // namespace nx::data_sync_engine
