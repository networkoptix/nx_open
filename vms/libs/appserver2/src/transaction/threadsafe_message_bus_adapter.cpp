// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "threadsafe_message_bus_adapter.h"

namespace ec2 {

void ThreadsafeMessageBusAdapter::start()
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::start();
}

void ThreadsafeMessageBusAdapter::stop()
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::stop();
}

QSet<nx::Uuid> ThreadsafeMessageBusAdapter::directlyConnectedClientPeers() const
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::directlyConnectedClientPeers();
}

nx::Uuid ThreadsafeMessageBusAdapter::routeToPeerVia(
    const nx::Uuid& dstPeer,
    int* distance,
    nx::network::SocketAddress* knownPeerAddress) const
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::routeToPeerVia(dstPeer, distance, knownPeerAddress);
}

int ThreadsafeMessageBusAdapter::distanceToPeer(const nx::Uuid& dstPeer) const
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::distanceToPeer(dstPeer);
}

void ThreadsafeMessageBusAdapter::addOutgoingConnectionToPeer(
    const nx::Uuid& id,
    nx::vms::api::PeerType peerType,
    const nx::utils::Url& url,
    std::optional<nx::network::http::Credentials> credentials,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::addOutgoingConnectionToPeer(
        id, peerType, url, std::move(credentials), std::move(adapterFunc));
}

void ThreadsafeMessageBusAdapter::removeOutgoingConnectionFromPeer(const nx::Uuid& id)
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::removeOutgoingConnectionFromPeer(id);
}

void ThreadsafeMessageBusAdapter::updateOutgoingConnection(
    const nx::Uuid& id, nx::network::http::Credentials credentials)
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::updateOutgoingConnection(id, std::move(credentials));
}

void ThreadsafeMessageBusAdapter::dropConnections()
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::dropConnections();
}

ConnectionInfos ThreadsafeMessageBusAdapter::connectionInfos() const
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::connectionInfos();
}

void ThreadsafeMessageBusAdapter::setHandler(ECConnectionNotificationManager* handler)
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::setHandler(handler);
}

void ThreadsafeMessageBusAdapter::removeHandler(ECConnectionNotificationManager* handler)
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    base_type::removeHandler(handler);
}

QnJsonTransactionSerializer* ThreadsafeMessageBusAdapter::jsonTranSerializer() const
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::jsonTranSerializer();
}

QnUbjsonTransactionSerializer* ThreadsafeMessageBusAdapter::ubjsonTranSerializer() const
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::ubjsonTranSerializer();
}

ConnectionGuardSharedState* ThreadsafeMessageBusAdapter::connectionGuardSharedState()
{
    NX_MUTEX_LOCKER guard(&m_mutex);
    return base_type::connectionGuardSharedState();
}

} // namespace ec2
