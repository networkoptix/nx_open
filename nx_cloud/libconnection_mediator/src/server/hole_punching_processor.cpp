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
    NX_LOGX(lm("connect request. from %1 to host %2, connection id %2").
        arg(connection->getSourceAddress().toString()).
        arg(request.destinationHostName).arg(request.connectSessionGuid),
        cl_logDEBUG2);

    const api::ResultCode validationResult = validateConnectRequest(
        connection,
        request);
    if (validationResult != api::ResultCode::ok)
        return completionHandler(validationResult, api::ConnectResponse());

    //preparing and saving connection context
    QnMutexLocker lk(&m_mutex);
    const auto connectionFsmIterAndFlag = m_ongoingConnections.emplace(
        request.connectSessionGuid,
        nullptr);
    if (!connectionFsmIterAndFlag.second)
    {
        NX_LOGX(lm("connect request retransmit: from %1, to %2, connection id %3").
            arg(connection->getSourceAddress().toString()).
            arg(request.destinationHostName).arg(request.connectSessionGuid),
            cl_logDEBUG1);
        //ignoring request, response will be sent by fsm
        return;
    }

    connectionFsmIterAndFlag.first->second = 
        std::make_unique<UDPHolePunchingConnectionInitiationFsm>(
            std::move(request.connectSessionGuid),
            std::bind(
                &HolePunchingProcessor::connectSessionFinished,
                this,
                std::move(connectionFsmIterAndFlag.first)));
                
    //launching connect FSM
    connectionFsmIterAndFlag.first->second->onConnectRequest(
        connection,
        std::move(request),
        std::move(completionHandler));
}

void HolePunchingProcessor::connectionResult(
    const ConnectionStrongRef& /*connection*/,
    api::ConnectionResultRequest /*request*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::ok);
}

api::ResultCode HolePunchingProcessor::validateConnectRequest(
    const ConnectionStrongRef& connection,
    const api::ConnectRequest& request)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::udp)
    {
        NX_LOGX(lm("connect request from %1: only UDP hole punching is supported for now").
            arg(connection->getSourceAddress().toString()),
            cl_logDEBUG1);
        return api::ResultCode::badRequest;
    }

    const auto targetPeerDataLocker = m_listeningPeerPool->
        findAndLockPeerDataByHostName(request.destinationHostName);
    if (!targetPeerDataLocker)
    {
        NX_LOGX(lm("connect request from %1 to unknown host %2").
            arg(connection->getSourceAddress().toString()).arg(request.destinationHostName),
            cl_logDEBUG1);
        return api::ResultCode::notFound;
    }
    const auto& targetPeerData = targetPeerDataLocker->value();

    if (!targetPeerData.isListening)
    {
        NX_LOGX(lm("connect request from %1 to host %2 not listening for connections").
            arg(connection->getSourceAddress().toString()).arg(request.destinationHostName),
            cl_logDEBUG1);
        return api::ResultCode::notFound;
    }

    if (((targetPeerData.connectionMethods & request.connectionMethods) &
        api::ConnectionMethod::udpHolePunching) == 0)
    {
        NX_LOGX(lm("connect request from %1 (connection methods %2) to host %3 "
            "(connection methods %4). No suitable connection method").
            arg(connection->getSourceAddress().toString()).arg(request.connectionMethods).
            arg(request.destinationHostName).arg(targetPeerData.connectionMethods),
            cl_logDEBUG1);
        return api::ResultCode::notFound;
    }

    const auto peerConnectionStrongRef = targetPeerData.peerConnection.lock();
    if (!peerConnectionStrongRef)
    {
        NX_LOGX(lm("connect request from %1 to host %2. No connection to the target peer...").
            arg(connection->getSourceAddress().toString()).arg(request.destinationHostName),
            cl_logDEBUG1);
        return api::ResultCode::notFound;
    }

    return api::ResultCode::ok;
}

void HolePunchingProcessor::connectSessionFinished(
    ConnectSessionsDictionary::iterator sessionIter)
{
    //TODO #ak
}

} // namespace hpm
} // namespace nx
