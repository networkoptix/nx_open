#pragma once

#include <memory>

#include <QtCore/QSettings>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/service.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/stoppable.h>

namespace nx {
namespace network {
namespace http {

class HttpStreamSocketServer;
class MessageDispatcher;

} // namespace nx
} // namespace network
} // namespace http

namespace nx {
namespace hpm {

class PeerRegistrator;

namespace conf { class Settings; }
namespace http { class Server; }

class Controller;
class ListeningPeerPool;
class StunServer;

class MediatorProcess:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    MediatorProcess(int argc, char **argv);

    std::vector<SocketAddress> httpEndpoints() const;
    std::vector<SocketAddress> stunEndpoints() const;
    ListeningPeerPool* listeningPeerPool() const;

    Controller& controller();
    const Controller& controller() const;

protected:
    virtual std::unique_ptr<nx::utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const nx::utils::AbstractServiceSettings& settings) override;

private:
    Controller* m_controller;
    StunServer* m_stunServer;
    http::Server* m_httpServer;

    QString getDataDirectory();
    int printHelp();
};

} // namespace hpm
} // namespace nx
