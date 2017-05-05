#pragma once

#include <memory>

#include <QtCore/QSettings>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/service.h>
#include <nx/utils/std/future.h>

#include <nx/utils/thread/stoppable.h>

namespace nx_http {

class HttpStreamSocketServer;
class MessageDispatcher;

} // namespace nx_http

namespace nx {
namespace hpm {

class PeerRegistrator;

namespace conf {

class Settings;

} // namespace conf

class BusinessLogicComposite;
class ListeningPeerPool;
class StunServer;

class MediatorProcess:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    MediatorProcess(int argc, char **argv);

    const std::vector<SocketAddress>& httpEndpoints() const;
    const std::vector<SocketAddress>& stunEndpoints() const;
    ListeningPeerPool* listeningPeerPool() const;

protected:
    virtual std::unique_ptr<nx::utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const nx::utils::AbstractServiceSettings& settings) override;

private:
    std::vector<SocketAddress> m_httpEndpoints;
    BusinessLogicComposite* m_businessLogicComposite;
    StunServer* m_stunServerComposite;

    QString getDataDirectory();
    int printHelp();
    bool launchHttpServerIfNeeded(
        const conf::Settings& settings,
        const PeerRegistrator& peerRegistrator,
        std::unique_ptr<nx_http::MessageDispatcher>* const httpMessageDispatcher,
        std::unique_ptr<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>* const
            multiAddressHttpServer);
};

} // namespace hpm
} // namespace nx
