#pragma once

#include "http/server.h"

namespace nx::cloud::storage::service {

namespace conf { namespace conf { class Settings; } }
class Controller;

class View
{
public:
    View(const conf::Settings& settings, Controller* controller);

    void listen();
    void stop();

    http::Server* httpServer();
    const http::Server* httpServer() const;

private:
    http::Server m_httpServer;
};

} // namespace nx::cloud::storage::service