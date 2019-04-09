#include "redirecting_hole_punching_processor.h"

#include <nx/network/address_resolver.h>

#include "hole_punching_processor.h"

namespace nx::hpm {

namespace {

static QString logRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    const api::ConnectRequest& request)
{
    return lm("from %1(%2) to %3, session id %4")
        .args(request.originatingPeerId, requestSourceDescriptor.sourceAddress,
            request.destinationHostName, request.connectSessionId);
}

} //namespace

RedirectingHolePunchingProcessor::RedirectingHolePunchingProcessor(
    AbstractCloudDataProvider* cloudData,
    HolePunchingProcessor* holePunchingProcessor,
    ListeningPeerDb* listeningPeerDb)
    :
    RequestProcessor(cloudData),
    m_holePunchingProcessor(holePunchingProcessor),
    m_listeningPeerDb(listeningPeerDb)
{
}

RedirectingHolePunchingProcessor::~RedirectingHolePunchingProcessor()
{
    nx::network::SocketGlobals::instance().addressResolver().cancel(this);
}

void RedirectingHolePunchingProcessor::connect(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
    m_holePunchingProcessor->connect(
        requestSourceDescriptor,
        request,
        [this, completionHandler = std::move(completionHandler),
        requestSourceDescriptor, request](
            api::ResultCode resultCode, api::ConnectResponse response)
    {
        if (resultCode == api::ResultCode::notFound)
        {
            return redirectToRemoteMediator(
                requestSourceDescriptor,
                std::move(request),
                std::move(response),
                std::move(completionHandler));
        }

        return completionHandler(std::move(resultCode), std::move(response));
    });
}

void RedirectingHolePunchingProcessor::redirectToRemoteMediator(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    api::ConnectResponse response,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
    QString connectRequestString = logRequest(requestSourceDescriptor, request);
    NX_VERBOSE(this, "Attempting redirection for connect request: %1", connectRequestString);

    m_listeningPeerDb->findMediatorByPeerDomain(
        request.destinationHostName.toStdString(),
        [this, destinationHostName = request.destinationHostName, response = std::move(response),
            completionHandler = std::move(completionHandler), connectRequestString](
                MediatorEndpoint endpoint) mutable
    {
        if (!validateMediatorEndpoint(endpoint))
        {
            NX_VERBOSE(this,
                "Failed to redirect stun connect request: %1 to remote mediator endpoint: %2",
                connectRequestString, endpoint);
            return completionHandler(api::ResultCode::notFound, std::move(response));
        }

        NX_VERBOSE(this, "Remote mediator lookup for %1 succeeded with endpoint: %2",
            destinationHostName, endpoint);

        resolveDomainName(
            std::move(response),
            std::move(endpoint),
            std::move(completionHandler));
    });
}

bool RedirectingHolePunchingProcessor::validateMediatorEndpoint(
    const MediatorEndpoint& endpoint) const
{
    if (endpoint.domainName.empty())
    {
        NX_VERBOSE(this, "Remote mediator lookup returned empty domain name");
        return false;
    }

    if (endpoint.stunUdpPort == MediatorEndpoint::kPortUnused)
    {
        NX_VERBOSE(this, "Remote mediator lookup returned invalid stun udp port");
        return false;
    }

    if (endpoint == m_listeningPeerDb->thisMediatorEndpoint())
    {
        NX_VERBOSE(this,
            "Remote mediator lookup returned \"this\" mediator: %1, but connection request already"
            " failed.", endpoint.toString());
        return false;
    }

    return true;
}

void RedirectingHolePunchingProcessor::resolveDomainName(
    api::ConnectResponse response,
    MediatorEndpoint endpoint,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
    NX_VERBOSE(this, "Attempting dns resolution of %1 to ip", endpoint.domainName);

    nx::network::SocketGlobals::instance().addressResolver().resolveAsync(
        endpoint.domainName,
        [this, response = std::move(response), endpoint,
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode errorCode, std::deque<network::AddressEntry> entries) mutable
        {
            if (errorCode != SystemError::noError)
            {
                NX_VERBOSE(this, "Error resolving ip address of %1: %2",
                    endpoint.domainName, SystemError::toString(errorCode));
                return completionHandler(api::ResultCode::notFound, std::move(response));
            }

            if (entries.empty())
            {
                NX_VERBOSE(this, "No Ip address resolved for %1:", endpoint.domainName);
                return completionHandler(api::ResultCode::notFound, std::move(response));
            }

            response.alternateMediatorEndpointStunUdp = entries.front().toEndpoint();
            response.alternateMediatorEndpointStunUdp->port = endpoint.stunUdpPort;

            NX_VERBOSE(this, "Dns lookup for %1 resolved to: %2. Using redirection endpoint: %3",
                endpoint.domainName, containerString(entries),
                *response.alternateMediatorEndpointStunUdp);

            return completionHandler(api::ResultCode::tryAlternate, std::move(response));
        },
        network::NatTraversalSupport::disabled,
        AF_INET,
        this);
}

} // namespace nx::hpm