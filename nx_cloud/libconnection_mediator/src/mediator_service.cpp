/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "mediator_service.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/udp_server.h>
#include <platform/process/current_process.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/guard.h>
#include <utils/common/systemerror.h>
#include <utils/common/app_info.h>

#include "http/get_listening_peer_list_handler.h"
#include "libconnection_mediator_app_info.h"
#include "listening_peer_pool.h"
#include "peer_registrator.h"
#include "mediaserver_api.h"
#include "server/hole_punching_processor.h"
#include "settings.h"
#include "statistics/stats_manager.h"

namespace nx {
namespace hpm {

MediatorProcess::MediatorProcess(int argc, char **argv):
    m_argc(argc),
    m_argv(argv),
    m_listeningPeerPool(nullptr)
{
}

void MediatorProcess::pleaseStop()
{
    m_processTerminationEvent.set_value();
}

void MediatorProcess::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

const std::vector<SocketAddress>& MediatorProcess::httpEndpoints() const
{
    return m_httpEndpoints;
}

const std::vector<SocketAddress>& MediatorProcess::stunEndpoints() const
{
    return m_stunEndpoints;
}

int MediatorProcess::exec()
{
    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopedGuard(
        [this, &processStartResult]
        {
            if (m_startedEventHandler)
                m_startedEventHandler(processStartResult);
        });

    conf::Settings settings;
    //parsing command line arguments
    settings.load(m_argc, (const char**) m_argv);
    if (settings.showHelp())
    {
        settings.printCmdLineArgsHelp();
        return 0;
    }

    nx::utils::log::initialize(
        settings.logging(), settings.general().dataDir,
        QnLibConnectionMediatorAppInfo::applicationDisplayName());

    if (settings.stun().addrToListenList.empty())
    {
        NX_LOGX( "No STUN address to listen", cl_logALWAYS );
        return 1;
    }

    nx::utils::TimerManager timerManager;
    std::unique_ptr<AbstractCloudDataProvider> cloudDataProvider;
    if (settings.cloudDB().runWithCloud)
    {
        cloudDataProvider = AbstractCloudDataProviderFactory::create(
            settings.cloudDB().endpoint.toStdString(),
            settings.cloudDB().user.toStdString(),
            settings.cloudDB().password.toStdString(),
            settings.cloudDB().updateInterval);
    }
    else
    {
        NX_LOGX( lit( "STUN Server is running without cloud (debug mode)" ), cl_logALWAYS );
    }

    stats::StatsManager statsManager(settings);

    //STUN handlers
    nx::stun::MessageDispatcher stunMessageDispatcher;
    MediaserverApi mediaserverApi(cloudDataProvider.get(), &stunMessageDispatcher);
    ListeningPeerPool listeningPeerPool;
    m_listeningPeerPool = &listeningPeerPool;
    PeerRegistrator listeningPeerRegistrator(
        settings,
        cloudDataProvider.get(),
        &stunMessageDispatcher,
        &listeningPeerPool);
    HolePunchingProcessor cloudConnectProcessor(
        settings,
        cloudDataProvider.get(),
        &stunMessageDispatcher,
        &listeningPeerPool,
        &statsManager.collector());

    //accepting STUN requests by both tcp and udt
    MultiAddressServer<stun::SocketServer> tcpStunServer(
        &stunMessageDispatcher,
        false,
        nx::network::NatTraversalSupport::disabled);

    if (!tcpStunServer.bind(settings.stun().addrToListenList))
    {
        NX_LOGX(lit("Can not bind to TCP addresses: %1")
            .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
        return 3;
    }

    m_stunEndpoints = tcpStunServer.endpoints();
    tcpStunServer.forEachListener(
        [&settings](stun::SocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.stun().keepAliveOptions);
        });

    MultiAddressServer<stun::UdpServer> udpStunServer(&stunMessageDispatcher);
    if (!udpStunServer.bind(m_stunEndpoints /*settings.stun().addrToListenList*/))
    {
        NX_LOGX(lit("Can not bind to UDP addresses: %1")
            .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
        return 4;
    }

    std::unique_ptr<nx_http::MessageDispatcher> httpMessageDispatcher;
    std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>>
        multiAddressHttpServer;

    launchHttpServerIfNeeded(
        settings,
        listeningPeerRegistrator,
        &httpMessageDispatcher,
        &multiAddressHttpServer);

    // process privilege reduction
    CurrentProcess::changeUser(settings.general().systemUserToRunUnder);

    if (!tcpStunServer.listen())
    {
        NX_LOGX(lit("Can not listen on TCP addresses %1")
            .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
        return 5;
    }

    if (!udpStunServer.listen())
    {
        NX_LOGX(lit("Can not listen on UDP addresses %1")
            .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
        return 6;
    }

    if (multiAddressHttpServer && !multiAddressHttpServer->listen())
    {
        NX_LOGX(lit("Can not listen on HTTP addresses %1. Running without HTTP server")
            .arg(containerString(settings.http().addrToListenList)), cl_logWARNING);
    }

    NX_LOGX(lit("STUN Server is listening on %1")
        .arg(containerString(settings.stun().addrToListenList)), cl_logALWAYS);

    processStartResult = true;
    triggerOnStartedEventHandlerGuard.fire();

    // This is actually the main loop.
    m_processTerminationEvent.get_future().wait();

    //stopping accepting incoming connections

    return 0;
}

ListeningPeerPool* MediatorProcess::listeningPeerPool() const
{
    return m_listeningPeerPool;
}

bool MediatorProcess::launchHttpServerIfNeeded(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    std::unique_ptr<nx_http::MessageDispatcher>* const httpMessageDispatcher,
    std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>>* const
        multiAddressHttpServer)
{
    if (settings.http().addrToListenList.empty())
        return true;

    NX_LOGX("Bringing up HTTP server", cl_logINFO);

    *httpMessageDispatcher = std::make_unique<nx_http::MessageDispatcher>();

    //registering HTTP handlers
    (*httpMessageDispatcher)->registerRequestProcessor<http::GetListeningPeerListHandler>(
        http::GetListeningPeerListHandler::kHandlerPath,
        [&]() { return std::make_unique<http::GetListeningPeerListHandler>(peerRegistrator); });
    
    *multiAddressHttpServer =
        std::make_unique<MultiAddressServer<nx_http::HttpStreamSocketServer>>(
            nullptr,    //TODO #ak add authentication 
            httpMessageDispatcher->get(),
            false,
            nx::network::NatTraversalSupport::disabled);

    auto& httpServer = *multiAddressHttpServer;
    if (!httpServer->bind(settings.http().addrToListenList))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to bind HTTP server to address ... . %1")
            .arg(SystemError::toString(osErrorCode)), cl_logERROR);
        return false;
    }

    m_httpEndpoints = httpServer->endpoints();
    httpServer->forEachListener(
        [&settings](nx_http::HttpStreamSocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.http().keepAliveOptions);
        });

    return true;
}

} // namespace hpm
} // namespace nx
