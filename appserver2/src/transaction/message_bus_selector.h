#pragma once

#include <transaction/transaction_message_bus.h>
#include <nx/p2p/p2p_message_bus.h>

namespace ec2 {

template<class T>
void sendTransaction(
    QnTransactionMessageBusBase* bus,
    const QnTransaction<T>& tran)
{
    if (auto p2pBus = dynamic_cast<nx::p2p::MessageBus*>(bus))
        p2pBus->sendTransaction(tran);
    else if (auto msgBus = dynamic_cast<QnTransactionMessageBus*>(bus))
        msgBus->sendTransaction(tran);
}

template<class T>
void sendTransaction(
    QnTransactionMessageBusBase* bus,
    const QnTransaction<T>& tran,
    const QnPeerSet& dstPeers)
{
    if (auto p2pBus = dynamic_cast<nx::p2p::MessageBus*>(bus))
        p2pBus->sendTransaction(tran, dstPeers);
    else if (auto msgBus = dynamic_cast<QnTransactionMessageBus*>(bus))
        msgBus->sendTransaction(tran, dstPeers);
}

template<class T>
void sendTransaction(
    QnTransactionMessageBusBase* bus,
    const QnTransaction<T>& tran,
    const QnUuid& peer)
{
    if (auto p2pBus = dynamic_cast<nx::p2p::MessageBus*>(bus))
        p2pBus->sendTransaction(tran, QnPeerSet() << peer);
    else if (auto msgBus = dynamic_cast<QnTransactionMessageBus*>(bus))
        msgBus->sendTransaction(tran, QnPeerSet() << peer);
    else
        NX_CRITICAL(false, "Not implemented");
}

} // namespace ec2
