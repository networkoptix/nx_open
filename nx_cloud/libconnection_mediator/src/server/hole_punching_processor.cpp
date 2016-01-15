/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include "hole_punching_processor.h"

#include <nx/network/stun/message_dispatcher.h>
#include <nx/utils/log/log.h>

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {

HolePunchingProcessor::HolePunchingProcessor(
    AbstractCloudDataProvider* cloudData,
    nx::stun::MessageDispatcher* dispatcher,
    ListeningPeerPool* const listeningPeerPool)
:
    RequestProcessor(cloudData),
    m_listeningPeerPool(listeningPeerPool)
{
    dispatcher->registerRequestProcessor(
        stun::cc::methods::connect,
        [this](const ConnectionStrongRef& connection, stun::Message message)
        {
            processRequestWithOutput(
                &HolePunchingProcessor::connect,
                this,
                std::move(connection),
                std::move(message));
        });

    dispatcher->registerRequestProcessor(
        stun::cc::methods::connectionResult,
        [this](const ConnectionStrongRef& connection, stun::Message message)
        {
            processRequestWithNoOutput(
                &HolePunchingProcessor::connectionResult,
                this,
                std::move(connection),
                std::move(message));
        });
}

void HolePunchingProcessor::connect(
    const ConnectionStrongRef& connection,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
    completionHandler(api::ResultCode::notImplemented, api::ConnectResponse());
}

void HolePunchingProcessor::connectionResult(
    const ConnectionStrongRef& /*connection*/,
    api::ConnectionResultRequest /*request*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::ok);
}


} // namespace hpm
} // namespace nx
