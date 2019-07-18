#include "view.h"

#include "../controller/controller.h"
#include "../settings.h"

namespace nx::cloud::storage::service {

View::View(const Settings& settings, Controller& controller):
    m_settings(settings),
    m_httpServer(settings)
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

HttpServer& View::httpServer()
{
    return m_httpServer;
}

const HttpServer& View::httpServer() const
{
    return m_httpServer;
}

} // namespace nx::cloud::storage::service