/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <map>
#include <memory>

#include <utils/common/systemerror.h>

#include "abstract_outgoing_tunnel_connection.h"
#include "abstract_tunnel_connector.h"
#include "nx/network/cloud/address_resolver.h"
#include "nx/network/aio/basic_pollable.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API CrossNatConnector
    :
    public aio::BasicPollable
{
public:
    CrossNatConnector(const AddressEntry& targetPeerAddress);
    virtual ~CrossNatConnector();

    void connect(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler);

    virtual void stopWhileInAioThread() override;

private:
    const AddressEntry m_targetPeerAddress;
    std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>> m_connectors;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> m_completionHandler;

    void CrossNatConnector::onConnectorFinished(
        CloudConnectType connectorType,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
};

}   //namespace cloud
}   //namespace network
}   //namespace nx
