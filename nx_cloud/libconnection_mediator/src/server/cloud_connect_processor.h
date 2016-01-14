/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/cloud/data/connection_result_data.h>

#include "../request_processor.h"


namespace nx {

namespace stun {
class MessageDispatcher;
}

namespace hpm {

class ListeningPeerPool;

/** Implements cross-nat connection mediation techniques
*/
class CloudConnectProcessor
:
    protected RequestProcessor
{
public:
    CloudConnectProcessor(
        AbstractCloudDataProvider* cloudData,
        nx::stun::MessageDispatcher* dispatcher,
        ListeningPeerPool* const listeningPeerPool);

    void connect(
        const ConnectionStrongRef& connection,
        stun::Message message);
    void connectionResult(
        const ConnectionStrongRef& connection,
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

private:
    ListeningPeerPool* const m_listeningPeerPool;
};

} // namespace hpm
} // namespace nx
