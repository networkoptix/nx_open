
#pragma once

#include "transaction_transport_base.h"


namespace ec2 {

class QnTransactionTransport
:
    public QnTransactionTransportBase
{
public:
    /** Initializer for incoming connection */
    QnTransactionTransport(
        const QnUuid& connectionGuid,
        ConnectionLockGuard connectionLockGuard,
        const ApiPeerData& localPeer,
        const ApiPeerData& remotePeer,
        QSharedPointer<AbstractStreamSocket> socket,
        ConnectionType::Type connectionType,
        const nx_http::Request& request,
        const QByteArray& contentEncoding,
        const Qn::UserAccessData &userAccessData);
    /** Initializer for outgoing connection */
    QnTransactionTransport(const ApiPeerData& localPeer);

    template<class T>
    void sendTransaction(const QnTransaction<T>& transaction, const QnTransactionTransportHeader& header)
    {
        if (remotePeer().peerType == Qn::PT_CloudServer &&
            transaction.transactionType != TransactionType::Cloud)
        {
            return;
        }

        auto remoteAccess = ec2::getTransactionDescriptorByTransaction(transaction)->checkRemotePeerAccessFunc(m_userAccessData, transaction.params);
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
        if (remotePeer().peerType == Qn::PT_CloudServer &&
            transaction.transactionType != TransactionType::Cloud)
        {
            return;
        }

        auto td = ec2::getTransactionDescriptorByTransaction(transaction);
        auto remoteAccess = td->checkRemotePeerAccessFunc(m_userAccessData, transaction.params);

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
            td->filterByReadPermissionFunc(m_userAccessData, filteredParams);
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

    const Qn::UserAccessData& getUserAccessData() const { return m_userAccessData; }

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;

private:
    const Qn::UserAccessData m_userAccessData;
};

}   // namespace ec2
