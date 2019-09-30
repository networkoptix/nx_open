#pragma once

#include "http/server.h"

namespace nx::cloud::storage::service {

namespace conf { class Settings; }
namespace controller { class Controller; }

namespace view {

class View
{
public:
    View(const conf::Settings& settings, controller::Controller* controller);

    void listen();
    void stop();

    http::Server* httpServer();
    const http::Server* httpServer() const;

private:
    http::Server m_httpServer;
};

} // namespace view
} // namespace nx::cloud::storage::service
