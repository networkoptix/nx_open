#pragma once

#include "../http/server.h"

namespace nx::cloud::storage::service {

class Settings;
class Controller;

class View
{
public:
    View(const Settings& settings, Controller* controller);

    void listen();
    void stop();

    http::Server& httpServer();
    const http::Server& httpServer() const;

private:
    /*const Settings& m_settings;*/
    http::Server m_httpServer;
};

} // namespace nx::cloud::storage::service