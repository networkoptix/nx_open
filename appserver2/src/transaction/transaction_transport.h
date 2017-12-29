
#pragma once

#include "transaction_descriptor.h"
#include "transaction_transport_base.h"
#include "transaction_message_bus_base.h"


namespace ec2 {

class QnTransactionTransport
:
    public QnTransactionTransportBase
{
public:
    /** Initializer for incoming connection */
    QnTransactionTransport(
        TransactionMessageBusBase* bus,
        const QnUuid& connectionGuid,
        ConnectionLockGuard connectionLockGuard,
        const ApiPeerData& localPeer,
        const ApiPeerData& remotePeer,
        QSharedPointer<nx::network::AbstractStreamSocket> socket,
        ConnectionType::Type connectionType,
        const nx::network::http::Request& request,
        const QByteArray& contentEncoding,
        const Qn::UserAccessData &userAccessData);
    /** Initializer for outgoing connection */
    QnTransactionTransport(
        TransactionMessageBusBase* bus,
        ConnectionGuardSharedState* const connectionGuardSharedState,
        const ApiPeerData& localPeer);
    ~QnTransactionTransport();

    /**
     * @note This method is non-blocking if called within internal socket's aio thread.
     */
    void close();

    template<class T>
    void sendTransaction(const QnTransaction<T>& transaction, const QnTransactionTransportHeader& header)
    {
        if (!shouldTransactionBeSentToPeer(transaction))
            return;

        auto remoteAccess = ec2::getTransactionDescriptorByTransaction(transaction)->checkRemotePeerAccessFunc(m_bus->commonModule(), m_userAccessData, transaction.params);
        if (remoteAccess == RemotePeerAccess::Forbidden)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Permission check failed while sending transaction %1 to peer %2")
                .arg(transaction.toString())
                .arg(remotePeer().id.toString()),
                cl_logDEBUG1);
            return;
        }
        sendTransactionImpl(transaction, header);
    }

    template<template<typename, typename> class Cont, typename Param, typename A>
    void sendTransaction(const QnTransaction<Cont<Param, A>>& transaction, const QnTransactionTransportHeader& header)
    {
        if (!shouldTransactionBeSentToPeer(transaction))
            return;

        auto td = ec2::getTransactionDescriptorByTransaction(transaction);
        auto remoteAccess = td->checkRemotePeerAccessFunc(m_bus->commonModule(), m_userAccessData, transaction.params);

        if (remoteAccess == RemotePeerAccess::Forbidden)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Permission check failed while sending transaction %1 to peer %2")
                .arg(transaction.toString())
                .arg(remotePeer().id.toString()),
                cl_logDEBUG1);
            return;
        }
        else if (remoteAccess == RemotePeerAccess::Partial)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Permission check PARTIALLY failed while sending transaction %1 to peer %2")
                .arg(transaction.toString())
                .arg(remotePeer().id.toString()),
                cl_logDEBUG1);

            Cont<Param, A> filteredParams = transaction.params;
            td->filterByReadPermissionFunc(m_bus->commonModule(), m_userAccessData, filteredParams);
            auto newTransaction = transaction;
            newTransaction.params = filteredParams;

            sendTransactionImpl(newTransaction, header);
        }
        sendTransactionImpl(transaction, header);
    }

    bool sendSerializedTransaction(
        Qn::SerializationFormat srcFormat,
        const QByteArray& serializedTran,
        const QnTransactionTransportHeader& _header);

    void setBeforeDestroyCallback(std::function<void()> ttFinishCallback);

    const Qn::UserAccessData& getUserAccessData() const { return m_userAccessData; }

    template<class T>
    void sendTransactionImpl(const QnTransaction<T> &transaction, const QnTransactionTransportHeader& _header)
    {
        QnTransactionTransportHeader header(_header);
        NX_ASSERT(header.processedPeers.contains(localPeer().id));
        header.fillSequence(localPeer().id, localPeer().instanceId);
#ifdef _DEBUG

        for (const QnUuid& peer : header.dstPeers)
        {
            NX_ASSERT(!peer.isNull());
            //NX_ASSERT(peer != localPeer().id);
        }
#endif
        NX_ASSERT(!transaction.isLocal() || remotePeer().isClient(), Q_FUNC_INFO, "Invalid transaction type to send!");
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("send transaction %1 to peer %2").arg(transaction.toString()).arg(remotePeer().id.toString()), cl_logDEBUG1);

        switch (remotePeer().dataFormat)
        {
        case Qn::JsonFormat:
            if (remotePeer().peerType == Qn::PT_OldMobileClient)
                addDataToTheSendQueue(m_bus->jsonTranSerializer()->serializedTransactionWithoutHeader(transaction) + QByteArray("\r\n"));
            else
                addDataToTheSendQueue(m_bus->jsonTranSerializer()->serializedTransactionWithHeader(transaction, header));
            break;
            //case Qn::BnsFormat:
            //    addDataToTheSendQueue(QnBinaryTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            //    break;
        case Qn::UbjsonFormat:
            addDataToTheSendQueue(m_bus->ubjsonTranSerializer()->serializedTransactionWithHeader(transaction, header));
            break;
        default:
            qWarning() << "Client has requested data in an unsupported format" << remotePeer().dataFormat;
            addDataToTheSendQueue(m_bus->ubjsonTranSerializer()->serializedTransactionWithHeader(transaction, header));
            break;
        }
    }
protected:
    virtual void fillAuthInfo(
        const nx::network::http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;

private:
    TransactionMessageBusBase* m_bus;
    const Qn::UserAccessData m_userAccessData;
    std::function<void()> m_beforeDestructionHandler;
};

}   // namespace ec2
