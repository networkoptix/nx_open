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

namespace nx {
namespace hpm {

namespace conf { class Settings; }
namespace stats { class AbstractCollector; }

class ListeningPeerPool;

/**
 * Handles requests used to establish hole punching connection.
 * Implements hole punching connection mediation techniques.
 */
class HolePunchingProcessor:
    protected RequestProcessor
{
public:
    HolePunchingProcessor(
        const conf::Settings& settings,
        AbstractCloudDataProvider* cloudData,
        ListeningPeerPool* listeningPeerPool,
        AbstractRelayClusterClient* relayClusterClient,
        stats::AbstractCollector* statisticsCollector);
    virtual ~HolePunchingProcessor();

    void stop();

    void connect(
        const ConnectionStrongRef& connection,
        api::ConnectRequest request,
        network::stun::Message requestMessage,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);

    void onConnectionAckRequest(
        const ConnectionStrongRef& connection,
        api::ConnectionAckRequest request,
        network::stun::Message requestMessage,
        std::function<void(api::ResultCode)> completionHandler);

    void connectionResult(
        const ConnectionStrongRef& connection,
        api::ConnectionResultRequest request,
        network::stun::Message requestMessage,
        std::function<void(api::ResultCode)> completionHandler);

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

    std::tuple<api::ResultCode, boost::optional<ListeningPeerPool::ConstDataLocker>>
        validateConnectRequest(
            const ConnectionStrongRef& connection,
            const api::ConnectRequest& request);

    void connectSessionFinished(
        ConnectSessionsDictionary::iterator,
        api::NatTraversalResultCode connectionResult);
};

} // namespace hpm
} // namespace nx
