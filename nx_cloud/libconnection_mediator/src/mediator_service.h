#pragma once

#include <memory>

#include <QtCore/QSettings>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>

#include <utils/common/stoppable.h>

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

class MediatorProcess:
    public QnStoppable
{
public:
    MediatorProcess(int argc, char **argv);

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);
    const std::vector<SocketAddress>& httpEndpoints() const;
    const std::vector<SocketAddress>& stunEndpoints() const;

    int exec();

private:
    std::unique_ptr<QSettings> m_settings;
    int m_argc;
    char** m_argv;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;
    std::vector<SocketAddress> m_httpEndpoints;
    std::vector<SocketAddress> m_stunEndpoints;
    nx::utils::promise<void> m_processTerminationEvent;

    QString getDataDirectory();
    int printHelp();
    bool launchHttpServerIfNeeded(
        const conf::Settings& settings,
        const PeerRegistrator& peerRegistrator,
        std::unique_ptr<nx_http::MessageDispatcher>* const httpMessageDispatcher,
        std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>>* const
            multiAddressHttpServer);
};

} // namespace hpm
} // namespace nx
