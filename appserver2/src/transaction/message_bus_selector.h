#pragma once

#include <transaction/transaction_message_bus.h>
#include <nx/p2p/p2p_message_bus.h>

namespace ec2 {

template<class T>
void sendTransaction(
    QnTransactionMessageBusBase* bus,
    const QnTransaction<T>& tran,
    const QnPeerSet& dstPeers = QnPeerSet())
{
    if (auto p2pBus = dynamic_cast<nx::p2p::P2pMessageBus*>(bus))
        p2pBus->sendTransaction<T>(tran, dstPeers);
    else if (auto msgBus = dynamic_cast<QnTransactionMessageBus*>(bus))
        msgBus->sendTransaction<T>(tran, dstPeers);
}

template <class T>
void sendTransaction(
    QnTransactionMessageBusBase* bus,
    const QnTransaction<T>& tran,
    const QnUuid& dstPeerId)
{
    dstPeerId.isNull() ? sendTransaction(bus, tran) : sendTransaction(bus, tran, QnPeerSet() << dstPeerId);
}

} // namespace ec2