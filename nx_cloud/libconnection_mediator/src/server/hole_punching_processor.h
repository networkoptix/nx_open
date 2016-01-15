/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>

#include "../request_processor.h"


namespace nx {

namespace stun {
class MessageDispatcher;
}

namespace hpm {

class ListeningPeerPool;

/** Implements hole punching connection mediation techniques
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

    void connect(
        const ConnectionStrongRef& connection,
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);
    void connectionResult(
        const ConnectionStrongRef& connection,
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

private:
    ListeningPeerPool* const m_listeningPeerPool;
};

} // namespace hpm
} // namespace nx
