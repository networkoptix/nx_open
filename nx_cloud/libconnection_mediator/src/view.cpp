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
        &controller->discoveredPeerPool()),
    m_stunServer(settings)
{
    // TODO: #ak Following call should be removed.
    // Http server should be passed to STUN server via constructor.
    m_stunServer.initializeHttpTunnelling(&m_httpServer);
    registerStunApiHandlers(controller);
}

void View::start()
{
    m_httpServer.listen();
    m_stunServer.listen();
}

std::vector<network::SocketAddress> View::httpEndpoints() const
{
    return m_httpServer.endpoints();
}

std::vector<network::SocketAddress> View::stunEndpoints() const
{
    return m_stunServer.endpoints();
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

    NX_ASSERT(result, Q_FUNC_INFO, "Could not register ping processor");
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
            [peerRegistrator](const ConnectionStrongRef& connection, network::stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::listen,
                    peerRegistrator,
                    std::move(connection),
                    std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::getConnectionState,
            [peerRegistrator](const ConnectionStrongRef& connection, network::stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::checkOwnState,
                    peerRegistrator,
                    std::move(connection),
                    std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::resolveDomain,
            [peerRegistrator](const ConnectionStrongRef& connection, network::stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::resolveDomain,
                    peerRegistrator,
                    std::move(connection),
                    std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::resolvePeer,
            [peerRegistrator](const ConnectionStrongRef& connection, network::stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::resolvePeer,
                    peerRegistrator,
                    std::move(connection),
                    std::move(message));
            }) &&

        dispatcher->registerRequestProcessor(
            network::stun::extension::methods::clientBind,
            [peerRegistrator](const ConnectionStrongRef& connection, network::stun::Message message)
            {
                processRequestWithOutput(
                    &PeerRegistrator::clientBind,
                    peerRegistrator,
                    std::move(connection),
                    std::move(message));
            }) ;

    NX_ASSERT(result, Q_FUNC_INFO, "Could not register one of PeerRegistrator methods");
}

void View::registerStunApiHandlers(
    HolePunchingProcessor* holePunchingProcessor,
    network::stun::MessageDispatcher* dispatcher)
{
    dispatcher->registerRequestProcessor(
        network::stun::extension::methods::connect,
        [holePunchingProcessor](
            const ConnectionStrongRef& connection,
            network::stun::Message message)
        {
            processRequestWithOutput(
                &HolePunchingProcessor::connect,
                holePunchingProcessor,
                std::move(connection),
                std::move(message));
        });

    dispatcher->registerRequestProcessor(
        network::stun::extension::methods::connectionAck,
        [holePunchingProcessor](
            const ConnectionStrongRef& connection,
            network::stun::Message message)
        {
            processRequestWithNoOutput(
                &HolePunchingProcessor::onConnectionAckRequest,
                holePunchingProcessor,
                std::move(connection),
                std::move(message));
        });

    dispatcher->registerRequestProcessor(
        network::stun::extension::methods::connectionResult,
        [holePunchingProcessor](
            const ConnectionStrongRef& connection,
            network::stun::Message message)
        {
            processRequestWithNoOutput(
                &HolePunchingProcessor::connectionResult,
                holePunchingProcessor,
                std::move(connection),
                std::move(message));
        });
}

} // namespace hpm
} // namespace nx
