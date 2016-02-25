/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>
#include <tuple>

#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/utils/thread/mutex.h>

#include "udp_hole_punching_connection_initiation_fsm.h"
#include "../request_processor.h"


namespace nx {

namespace stun {
class MessageDispatcher;
}

namespace hpm {

class ListeningPeerPool;

/** Handles requests used to establish hole punching connection. 
    Implements hole punching connection mediation techniques
*/
class HolePunchingProcessor
:
    protected RequestProcessor
{
public:
    HolePunchingProcessor(
        AbstractCloudDataProvider* cloudData,
        nx::stun::MessageDispatcher* dispatcher,
        ListeningPeerPool* const listeningPeerPool);
    virtual ~HolePunchingProcessor();

    void connect(
        const ConnectionStrongRef& connection,
        api::ConnectRequest request,
        stun::Message requestMessage,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);
    void onConnectionAckRequest(
        const ConnectionStrongRef& connection,
        api::ConnectionAckRequest request,
        stun::Message requestMessage,
        std::function<void(api::ResultCode)> completionHandler);
    void connectionResult(
        const ConnectionStrongRef& connection,
        api::ConnectionResultRequest request,
        stun::Message requestMessage,
        std::function<void(api::ResultCode)> completionHandler);

private:
    typedef std::map<
        nx::String,
        std::unique_ptr<UDPHolePunchingConnectionInitiationFsm>
    > ConnectSessionsDictionary;

    ListeningPeerPool* const m_listeningPeerPool;
    QnMutex m_mutex;
    //map<id, connection initiation>
    ConnectSessionsDictionary m_activeConnectSessions;

    std::tuple<api::ResultCode, boost::optional<ListeningPeerPool::ConstDataLocker>>
        validateConnectRequest(
            const ConnectionStrongRef& connection,
            const api::ConnectRequest& request);

    void connectSessionFinished(
        ConnectSessionsDictionary::iterator,
        api::ResultCode connectionResult);
};

} // namespace hpm
} // namespace nx
