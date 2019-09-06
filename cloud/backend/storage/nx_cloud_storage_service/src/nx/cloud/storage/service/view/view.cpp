#include "view.h"

#include "nx/cloud/storage/service/controller/controller.h"
#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::view {

View::View(const conf::Settings& settings, controller::Controller* controller):
    m_httpServer(settings, controller)
{
}

void View::listen()
{
    if (!m_httpServer.bind())
        throw std::runtime_error("HTTP server failed to bind");

    if (!m_httpServer.listen())
        throw std::runtime_error("HTTP server faild to listen");

    NX_INFO(this, "HTTP server listening on %1, ssl/%2",
        containerString(m_httpServer.httpEndpoints()),
        containerString(m_httpServer.httpsEndpoints()));
}

void View::stop()
{
    NX_INFO(this, "Stopping View...");

    m_httpServer.stop();

    NX_INFO(this, "View stopped");
}

http::Server* View::httpServer()
{
    return &m_httpServer;
}

const http::Server* View::httpServer() const
{
    return &m_httpServer;
}

} // namespace nx::cloud::storage::service::view
