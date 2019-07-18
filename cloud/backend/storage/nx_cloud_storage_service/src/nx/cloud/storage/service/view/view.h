#pragma once

#include "http_server.h"

namespace nx::cloud::storage::service {

class Settings;
class Controller;

class View
{

public:
    View(const Settings& settings, Controller& controller);

    void listen();
    void stop();

    HttpServer& httpServer();
    const HttpServer& httpServer() const;

private:
    const Settings& m_settings;
    HttpServer m_httpServer;
};

} // namespace nx::cloud::storage::service