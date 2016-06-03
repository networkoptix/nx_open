/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef HOLE_PUNCHER_SERVICE_H
#define HOLE_PUNCHER_SERVICE_H

#include <memory>

#include <QtCore/QSettings>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/move_only_func.h>

#include <utils/common/stoppable.h>


namespace nx_http {
    class HttpStreamSocketServer;
    class MessageDispatcher;
}   // namespace nx_http

namespace nx {
namespace hpm {

class ListeningPeerPool;

namespace conf {
    class Settings;
}   // namespace conf

class MediatorProcess
:
    public QtService<QtSingleCoreApplication>,
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

protected:
    virtual int executeApplication() override;
    virtual void start() override;
    virtual void stop() override;

private:
    std::unique_ptr<QSettings> m_settings;
    int m_argc;
    char** m_argv;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;
    std::vector<SocketAddress> m_httpEndpoints;
    std::vector<SocketAddress> m_stunEndpoints;

    QString getDataDirectory();
    int printHelp();
    void initializeLogging(const conf::Settings& settings);
    bool launchHttpServerIfNeeded(
        const conf::Settings& settings,
        const ListeningPeerPool& listeningPeerPool,
        std::unique_ptr<nx_http::MessageDispatcher>* const httpMessageDispatcher,
        std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>>* const
            multiAddressHttpServer);
};

} // namespace hpm
} // namespace nx

#endif  //HOLE_PUNCHER_SERVICE_H
