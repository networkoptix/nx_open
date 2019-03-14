#include "view.h"

#include "controller.h"
#include "peer_registrator.h"
#include "server/hole_punching_processor.h"
#include "server/stun_request_processing_helper.h"

namespace nx {
namespace hpm {

View::View(
    const conf::Settings& settings,
    Controller* controller)
    :
    m_httpServer(
        settings,
        controller->listeningPeerRegistrator(),
        &controller->discoveredPeerPool(),
        &controller->cloudConnectProcessor()),
    m_stunServer(settings, &m_httpServer)
{
    registerStunApiHandlers(controller);
}

void View::start()
{
    m_httpServer.listen();
    m_stunServer.listen();
}

void View::stop()
{
    m_stunServer.stopAcceptingNewRequests();
    m_httpServer.stopAcceptingNewRequests();
}

const http::Server& View::httpServer() const
{
    return m_httpServer;
}

http::Server& View::httpServer()
{
    return m_httpServer;
}

const StunServer& View::stunServer() const
{
    return m_stunServer;
}

void View::registerStunApiHandlers(Controller* controller)
{
    registerStunApiHandlers(
        &controller->mediaserverEndpointTester(),
        &m_stunServer.dispatcher());

    registerStunApiHandlers(
        &controller->listeningPeerRegistrator(),
        &m_stunServer.dispatcher());

    registerStunApiHandlers(
        &controller->cloudConnectProcessor(),
        &m_stunServer.dispatcher());
}

void View::registerStunApiHandlers(
    MediaserverEndpointTesterBase* mediaserverEndpointTester,
    network::stun::MessageDispatcher* dispatcher)
{
    const auto result =
        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::ping,
            [mediaserverEndpointTester](
                ConnectionStrongRef connection,
                network::stun::Message message)
            {
                mediaserverEndpointTester->ping(
                    std::move(connection), std::move(message));
            });

    NX_ASSERT(result, "Could not register ping processor");
}

void View::registerStunApiHandlers(
    PeerRegistrator* peerRegistrator,
    network::stun::MessageDispatcher* dispatcher)
{
    const auto result =
        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::bind,
            [peerRegistrator](const ConnectionStrongRef& connection, network::stun::Message message)
            {
                peerRegistrator->bind(std::move(connection), std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::listen,
            detail::createRequestProcessor(&PeerRegistrator::listen, peerRegistrator)) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::getConnectionState,
            detail::createRequestProcessor(&PeerRegistrator::checkOwnState, peerRegistrator)) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::resolveDomain,
            detail::createRequestProcessor(&PeerRegistrator::resolveDomain, peerRegistrator)) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::resolvePeer,
            detail::createRequestProcessor(&PeerRegistrator::resolvePeer, peerRegistrator)) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::clientBind,
            detail::createRequestProcessor(&PeerRegistrator::clientBind, peerRegistrator));

    NX_ASSERT(result, "Could not register one of PeerRegistrator methods");
}

void View::registerStunApiHandlers(
    HolePunchingProcessor* holePunchingProcessor,
    network::stun::MessageDispatcher* dispatcher)
{
    dispatcher->registerRequestProcessor(
        network::stun::extension::methods::connect,
        detail::createRequestProcessor(
            &HolePunchingProcessor::connect,
            holePunchingProcessor));

    dispatcher->registerRequestProcessor(
        network::stun::extension::methods::connectionAck,
        detail::createRequestProcessor(
            &HolePunchingProcessor::onConnectionAckRequest,
            holePunchingProcessor));

    dispatcher->registerRequestProcessor(
        network::stun::extension::methods::connectionResult,
        detail::createRequestProcessor(
            &HolePunchingProcessor::connectionResult,
            holePunchingProcessor));
}

} // namespace hpm
} // namespace nx
