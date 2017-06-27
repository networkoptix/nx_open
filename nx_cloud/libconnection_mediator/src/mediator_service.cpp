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
#include <nx/utils/platform/current_process.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

#include "business_logic_composite.h"
#include "http/get_listening_peer_list_handler.h"
#include "libconnection_mediator_app_info.h"
#include "settings.h"
#include "statistics/stats_manager.h"
#include "stun_server.h"

namespace nx {
namespace hpm {

MediatorProcess::MediatorProcess(int argc, char **argv):
    base_type(argc, argv, QnLibConnectionMediatorAppInfo::applicationDisplayName()),
    m_businessLogicComposite(nullptr),
    m_stunServerComposite(nullptr)
{
}

const std::vector<SocketAddress>& MediatorProcess::httpEndpoints() const
{
    return m_httpEndpoints;
}

const std::vector<SocketAddress>& MediatorProcess::stunEndpoints() const
{
    return m_stunServerComposite->endpoints();
}

ListeningPeerPool* MediatorProcess::listeningPeerPool() const
{
    return &m_businessLogicComposite->listeningPeerPool();
}

std::unique_ptr<nx::utils::AbstractServiceSettings> MediatorProcess::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int MediatorProcess::serviceMain(const nx::utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    if (settings.stun().addrToListenList.empty())
    {
        NX_LOGX("No STUN address to listen", cl_logALWAYS);
        return 1;
    }

    nx::utils::TimerManager timerManager;

    auto stunServerComposite = std::make_unique<StunServer>(settings);
    if (!stunServerComposite->bind())
        return 3;
    m_stunServerComposite = stunServerComposite.get();

    // TODO: #ak BusinessLogicComposite should be instanciated before stunServerComposite.
    // That requires decoupling businessLogicComposite and STUN request handling.
    BusinessLogicComposite businessLogicComposite(
        settings,
        &stunServerComposite->dispatcher());
    m_businessLogicComposite = &businessLogicComposite;

    // TODO: #ak create http server composite class.
    std::unique_ptr<nx_http::MessageDispatcher> httpMessageDispatcher;
    std::unique_ptr<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>
        multiAddressHttpServer;

    launchHttpServerIfNeeded(
        settings,
        businessLogicComposite.listeningPeerRegistrator(),
        &httpMessageDispatcher,
        &multiAddressHttpServer);

    // process privilege reduction
    nx::utils::CurrentProcess::changeUser(settings.general().systemUserToRunUnder);

    if (!stunServerComposite->listen())
        return 5;

    if (multiAddressHttpServer && !multiAddressHttpServer->listen())
    {
        NX_LOGX(lit("Can not listen on HTTP addresses %1. Running without HTTP server")
            .arg(containerString(settings.http().addrToListenList)), cl_logWARNING);
    }

    NX_LOGX(lit("STUN Server is listening on %1")
        .arg(containerString(settings.stun().addrToListenList)), cl_logALWAYS);

    const int result = runMainLoop();

    stunServerComposite->stopAcceptingNewRequests();
    businessLogicComposite.stop();
    stunServerComposite.reset();

    NX_LOGX(lit("%1 is stopped")
        .arg(QnLibConnectionMediatorAppInfo::applicationDisplayName()), cl_logALWAYS);

    return result;
}

bool MediatorProcess::launchHttpServerIfNeeded(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    std::unique_ptr<nx_http::MessageDispatcher>* const httpMessageDispatcher,
    std::unique_ptr<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>* const multiAddressHttpServer)
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
        std::make_unique<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>(
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
