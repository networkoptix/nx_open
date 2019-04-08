#pragma once

#include <functional>
#include <tuple>

#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/utils/thread/mutex.h>

#include "udp_hole_punching_connection_initiation_fsm.h"
#include "../relay/abstract_relay_cluster_client.h"
#include "../request_processor.h"
#include "../listening_peer_db.h"

namespace nx {
namespace hpm {

namespace conf { class Settings; }
namespace stats { class AbstractCollector; }

class ListeningPeerPool;

//-------------------------------------------------------------------------------------------------
// HolePunchingProcessor

/**
 * Handles requests used to establish hole punching connection.
 * Implements hole punching connection mediation techniques.
 */
class HolePunchingProcessor:
    protected RequestProcessor
{
public:
    class ConnectHandler:
        protected RequestProcessor
    {
    public:
        ConnectHandler(
            AbstractCloudDataProvider* cloudData,
            HolePunchingProcessor* holePunchingProcessor,
            ListeningPeerDb* listeningPeerDb);
        ~ConnectHandler();

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

public:
    HolePunchingProcessor(
        const conf::Settings& settings,
        AbstractCloudDataProvider* cloudData,
        ListeningPeerDb* listeningPeerDb,
        ListeningPeerPool* listeningPeerPool,
        AbstractRelayClusterClient* relayClusterClient,
        stats::AbstractCollector* statisticsCollector);
    virtual ~HolePunchingProcessor();

    void stop();

    void connect(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);

    void onConnectionAckRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectionAckRequest request,
        std::function<void(api::ResultCode)> completionHandler);

    void connectionResult(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

    ConnectHandler& connectHandler();

private:
    typedef std::map<
        nx::String,
        std::unique_ptr<UDPHolePunchingConnectionInitiationFsm>
    > ConnectSessionsDictionary;

    const conf::Settings& m_settings;
    ListeningPeerPool* m_listeningPeerPool;
    AbstractRelayClusterClient* m_relayClusterClient;
    stats::AbstractCollector* m_statisticsCollector;
    QnMutex m_mutex;
    //map<id, connection initiation>
    ConnectSessionsDictionary m_activeConnectSessions;
    ConnectHandler m_connectHandler;

    std::tuple<api::ResultCode, boost::optional<ListeningPeerPool::ConstDataLocker>>
        validateConnectRequest(
            const RequestSourceDescriptor& requestSourceDescriptor,
            const api::ConnectRequest& request);

    void connectSessionFinished(
        ConnectSessionsDictionary::iterator,
        api::NatTraversalResultCode connectionResult);
};

} // namespace hpm
} // namespace nx
