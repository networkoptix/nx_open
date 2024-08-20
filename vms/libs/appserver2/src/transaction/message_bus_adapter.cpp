// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_bus_adapter.h"
#include <nx/p2p/p2p_message_bus.h>

using namespace nx::vms;

namespace ec2 {

TransactionMessageBusAdapter::TransactionMessageBusAdapter(
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    AbstractTransactionMessageBus(commonModule),
    m_jsonTranSerializer(jsonTranSerializer),
    m_ubjsonTranSerializer(ubjsonTranSerializer)
{
}

void TransactionMessageBusAdapter::reset()
{
    m_bus.reset();
}

void TransactionMessageBusAdapter::initInternal()
{
    connect(m_bus.get(), &AbstractTransactionMessageBus::peerFound,
        this, &AbstractTransactionMessageBus::peerFound, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::peerLost,
        this, &AbstractTransactionMessageBus::peerLost, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::remotePeerUnauthorized,
        this, &AbstractTransactionMessageBus::remotePeerUnauthorized, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::remotePeerForbidden,
        this, &AbstractTransactionMessageBus::remotePeerForbidden, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::remotePeerHandshakeError,
        this, &AbstractTransactionMessageBus::remotePeerHandshakeError);
}

void TransactionMessageBusAdapter::start()
{
    m_bus->start();
}

void TransactionMessageBusAdapter::stop()
{
    m_bus->stop();
}

QSet<nx::Uuid> TransactionMessageBusAdapter::directlyConnectedClientPeers() const
{
    return m_bus->directlyConnectedClientPeers();
}

QSet<nx::Uuid> TransactionMessageBusAdapter::directlyConnectedServerPeers() const
{
    return m_bus->directlyConnectedServerPeers();
}

nx::Uuid TransactionMessageBusAdapter::routeToPeerVia(
    const nx::Uuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const
{
    // This method can be called asynchronously after connection has been closed from the
    // QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer (QtConcurrent call).
    return m_bus ? m_bus->routeToPeerVia(dstPeer, distance, knownPeerAddress) : nx::Uuid();
}

int TransactionMessageBusAdapter::distanceToPeer(const nx::Uuid& dstPeer) const
{
    return m_bus->distanceToPeer(dstPeer);
}

void TransactionMessageBusAdapter::addOutgoingConnectionToPeer(
    const nx::Uuid& id,
    nx::vms::api::PeerType peerType,
    const nx::utils::Url &url,
    std::optional<nx::network::http::Credentials> credentials,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    m_bus->addOutgoingConnectionToPeer(
        id, peerType, url, std::move(credentials), std::move(adapterFunc));
}

void TransactionMessageBusAdapter::removeOutgoingConnectionFromPeer(const nx::Uuid& id)
{
    m_bus->removeOutgoingConnectionFromPeer(id);
}

void TransactionMessageBusAdapter::updateOutgoingConnection(
    const nx::Uuid& id, nx::network::http::Credentials credentials)
{
    m_bus->updateOutgoingConnection(id, credentials);
}

void TransactionMessageBusAdapter::dropConnections()
{
    m_bus->dropConnections();
}

ConnectionInfos TransactionMessageBusAdapter::connectionInfos() const
{
    return m_bus->connectionInfos();
}

void TransactionMessageBusAdapter::setHandler(ECConnectionNotificationManager* handler)
{
    m_bus->setHandler(handler);
}

void TransactionMessageBusAdapter::removeHandler(ECConnectionNotificationManager* handler)
{
    m_bus->removeHandler(handler);
}

QnJsonTransactionSerializer* TransactionMessageBusAdapter::jsonTranSerializer() const
{
    return m_bus->jsonTranSerializer();
}

QnUbjsonTransactionSerializer* TransactionMessageBusAdapter::ubjsonTranSerializer() const
{
    return m_bus->ubjsonTranSerializer();
}

ConnectionGuardSharedState* TransactionMessageBusAdapter::connectionGuardSharedState()
{
    return m_bus->connectionGuardSharedState();
}

ec2::AbstractTransactionMessageBus* TransactionMessageBusAdapter::getBus()
{
    return m_bus.get();
}

} // namespace ec2
