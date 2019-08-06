#pragma once

#include "../request_processor.h"

#include <nx/network/cloud/data/connect_data.h>

#include "../listening_peer_db.h"

namespace nx::hpm {

class AbstractCloudDataProvider;
class HolePunchingProcessor;

/**
 * Attempts to redirect stun connect requests to a remote mediator instance if "this" mediator's
 * HolePunchingProcessor fails to connect to the requested media server.
 */
class RedirectingHolePunchingProcessor:
    protected RequestProcessor
{
public:
    RedirectingHolePunchingProcessor(
        AbstractCloudDataProvider* cloudData,
        HolePunchingProcessor* holePunchingProcessor,
        ListeningPeerDb* listeningPeerDb);
    ~RedirectingHolePunchingProcessor();

    void connect(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);

private:
    void redirectToRemoteMediator(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        api::ConnectResponse reponse,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);

    bool validateMediatorEndpoint(const MediatorEndpoint& endpoint) const;

    void resolveDomainName(
        api::ConnectResponse response,
        MediatorEndpoint endpoint,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);

private:
    HolePunchingProcessor* m_holePunchingProcessor;
    ListeningPeerDb* m_listeningPeerDb;
};

} //namespace nx::hpm