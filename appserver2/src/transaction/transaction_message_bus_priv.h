#include <nx/p2p/p2p_connection_base.h>
#include <nx/p2p/p2p_serialization.h>
#include <utils/common/warnings.h>
#include <transaction/transaction_message_bus.h>

#include "ubjson_transaction_serializer.h"

namespace ec2 {

typedef std::function<bool(Qn::SerializationFormat, const QByteArray&)> FastFunctionType;

//Overload for ubjson transactions
template<typename Function, typename Param>
bool handleTransactionParams(
    TransactionMessageBusBase* bus,
    const QByteArray &serializedTransaction,
    QnUbjsonReader<QByteArray> *stream,
    const QnAbstractTransaction &abstractTransaction,
    Function function,
    FastFunctionType fastFunction)
{
    if (fastFunction(Qn::UbjsonFormat, serializedTransaction))
    {
        return true; // process transaction directly without deserialize
    }

    auto transaction = QnTransaction<Param>(abstractTransaction);
    if (!QnUbjson::deserialize(stream, &transaction.params))
    {
        qWarning() << "Can't deserialize transaction " << toString(abstractTransaction.command);
        return false;
    }
    if (!abstractTransaction.persistentInfo.isNull())
        bus->ubjsonTranSerializer()->addToCache(abstractTransaction.persistentInfo, abstractTransaction.command, serializedTransaction);
    function(transaction);
    return true;
}

//Overload for json transactions
template<typename Function, typename Param>
bool handleTransactionParams(
    TransactionMessageBusBase* /*bus*/,
    const QByteArray &serializedTransaction,
    const QJsonObject& jsonData,
    const QnAbstractTransaction &abstractTransaction,
    Function function,
    FastFunctionType fastFunction)
{
    if (fastFunction(Qn::JsonFormat, serializedTransaction))
    {
        return true; // process transaction directly without deserialize
    }
    auto transaction = QnTransaction<Param>(abstractTransaction);
    if (!QJson::deserialize(jsonData["params"], &transaction.params))
    {
        qWarning() << "Can't deserialize transaction " << toString(abstractTransaction.command);
        return false;
    }
    function(transaction);
    return true;
}

#define HANDLE_TRANSACTION_PARAMS_APPLY(_, value, param, ...) \
    case ApiCommand::value : \
        return handleTransactionParams<Function, param>(bus, serializedTransaction, serializationSupport, transaction, function, fastFunction);

template<typename SerializationSupport, typename Function>
bool handleTransaction2(
    TransactionMessageBusBase* bus,
    const QnAbstractTransaction& transaction,
    const SerializationSupport& serializationSupport,
    const QByteArray& serializedTransaction,
    const Function& function,
    FastFunctionType fastFunction)
{
    switch (transaction.command)
    {
        TRANSACTION_DESCRIPTOR_LIST(HANDLE_TRANSACTION_PARAMS_APPLY)
    default:
        NX_ASSERT(0, "Unknown transaction command");
    }
    return false;
}

#undef HANDLE_TRANSACTION_PARAMS_APPLY

template<class Function>
bool handleTransaction(
    TransactionMessageBusBase* bus,
    Qn::SerializationFormat tranFormat,
    const QByteArray &serializedTransaction,
    const Function &function,
    FastFunctionType fastFunction)
{
    if (tranFormat == Qn::UbjsonFormat)
    {
        QnAbstractTransaction transaction;
        QnUbjsonReader<QByteArray> stream(&serializedTransaction);
        if (!QnUbjson::deserialize(&stream, &transaction))
        {
            qnWarning("Ignore bad transaction data. size=%1.", serializedTransaction.size());
            return false;
        }

        return handleTransaction2(
            bus,
            transaction,
            &stream,
            serializedTransaction,
            function,
            fastFunction);
    }
    else if (tranFormat == Qn::JsonFormat)
    {
        QnAbstractTransaction transaction;
        QJsonObject tranObject;
        //TODO #ak take tranObject from cache
        if (!QJson::deserialize(serializedTransaction, &tranObject))
            return false;
        if (!QJson::deserialize(tranObject["tran"], &transaction))
            return false;

        return handleTransaction2(
            bus,
            transaction,
            tranObject["tran"].toObject(),
            serializedTransaction,
            function,
            fastFunction);
    }
    else
    {
        return false;
    }
}

template<typename MessageBus, typename Function>
bool handleTransactionWithHeader(
    MessageBus* bus,
    const nx::p2p::P2pConnectionPtr& connection,
    const QByteArray& data,
    Function function)
{
    int headerSize = 0;
    nx::p2p::TransportHeader header;
    if (connection->remotePeer().dataFormat == Qn::UbjsonFormat)
        header = nx::p2p::deserializeTransportHeader(data, &headerSize);
    else
        header.dstPeers.push_back(bus->localPeer().id);

    using namespace std::placeholders;

    return handleTransaction(
        bus,
        connection->remotePeer().dataFormat,
        data.mid(headerSize),
        std::bind(function, bus, _1, connection, header),
        [](Qn::SerializationFormat, const QByteArray&) { return false; });

    return true;
}

template<typename T>
void QnTransactionMessageBus::proxyTransaction(
    const QnTransaction<T>& tran,
    const QnTransactionTransportHeader& transportHeader)
{
    if (nx::vms::api::PeerData::isClient(m_localPeerType))
        return;

    auto newTransportHeader = transportHeader;
    ++newTransportHeader.distance;
    if (newTransportHeader.flags.testFlag(Qn::TT_ProxyToClient))
    {
        const auto clients = aliveClientPeers().keys().toSet();
        if (clients.isEmpty())
            return;

        newTransportHeader.dstPeers = clients;
        newTransportHeader.processedPeers += clients;
        newTransportHeader.processedPeers += commonModule()->moduleGUID();

        for (QnTransactionTransport* transport: m_connections)
        {
            if (transport->remotePeer().isClient() && transport->isReadyToSend(tran.command))
                transport->sendTransaction(tran, newTransportHeader);
        }

        return;
    }

    // Proxy incoming transaction to other peers.
    if (!newTransportHeader.dstPeers.isEmpty()
        && (newTransportHeader.dstPeers - newTransportHeader.processedPeers).isEmpty())
    {
        return; //< All dstPeers are already processed.
    }

    const auto oldProcessedPeers = newTransportHeader.processedPeers;

    // Do not put clients peers to processed list in case if client just reconnected to other
    // server and previous server hasn't got update yet.
    newTransportHeader.processedPeers += connectedServerPeers();
    newTransportHeader.processedPeers += commonModule()->moduleGUID();

    QSet<QnUuid> proxyList;
    for (QnTransactionTransport* transport: m_connections)
    {
        const auto remoteId = transport->remotePeer().id;
        if (oldProcessedPeers.contains(remoteId) || !transport->isReadyToSend(tran.command))
            continue;

        transport->sendTransaction(tran, newTransportHeader);
        proxyList += remoteId;
    }

    if (!proxyList.isEmpty()
        && nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG, lm("proxy transaction %1 to %2").args(
            tran.toString(), proxyList));
    }
}

template<typename T>
bool QnTransactionMessageBus::processSpecialTransaction(
    const QnTransaction<T>& tran,
    QnTransactionTransport* sender,
    const QnTransactionTransportHeader& transportHeader)
{
    QnMutexLocker lock(&m_mutex);

    // Do not perform any logic (aka sequence update) for foreign transaction. Just proxy.
    if (!transportHeader.dstPeers.isEmpty()
        && !transportHeader.dstPeers.contains(commonModule()->moduleGUID()))
    {
        if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
        {
            NX_DEBUG(QnLog::EC2_TRAN_LOG, lm("skip transaction %1 %2 for peers %3").args(
                tran.toString(), toString(transportHeader), transportHeader.dstPeers));
        }

        proxyTransaction(tran, transportHeader);
        return true;
    }

    updateLastActivity(sender, transportHeader);

    const auto transactionDescriptor = getTransactionDescriptorByTransaction(tran);
    const auto transactionHash =
        transactionDescriptor ? transactionDescriptor->getHashFunc(tran.params) : QnUuid();

    if (!checkSequence(transportHeader, tran, sender))
        return true;

    if (!sender->isReadSync(tran.command))
    {
        printTransaction("reject transaction (no readSync)",
            tran, transactionHash, transportHeader, sender);
        return true;
    }

    if (tran.isLocal() && nx::vms::api::PeerData::isServer(m_localPeerType))
    {
        printTransaction("reject local transaction",
            tran, transactionHash, transportHeader, sender);
        return true;
    }

    printTransaction("got transaction", tran, transactionHash, transportHeader, sender);

    // Process system transactions.
    switch (tran.command)
    {
        case ApiCommand::lockRequest:
        case ApiCommand::lockResponse:
        case ApiCommand::unlockRequest:
            onGotDistributedMutexTransaction(tran);
            break;
        case ApiCommand::tranSyncRequest:
            onGotTransactionSyncRequest(sender, tran);
            return true; //< Do not proxy.
        case ApiCommand::tranSyncResponse:
            onGotTransactionSyncResponse(sender, tran);
            return true; //< Do not proxy.
        case ApiCommand::tranSyncDone:
            onGotTransactionSyncDone(sender, tran);
            return true; //< Do not proxy.
        case ApiCommand::peerAliveInfo:
            onGotServerAliveInfo(tran, sender, transportHeader);
            return true; //< Do not proxy. This call contains built in proxy.
        case ApiCommand::runtimeInfoChanged:
            if (!onGotServerRuntimeInfo(tran, sender, transportHeader))
                return true; //< Already processed. Do not proxy and ignore the transaction.
            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            break;
        case ApiCommand::updatePersistentSequence:
            updatePersistentMarker(tran);
            break;
        case ApiCommand::installUpdate:
        case ApiCommand::uploadUpdate:
        case ApiCommand::changeSystemId:
        {
            // Transactions listed here should not go to the DbManager.
            // We are only interested in relevant notifications triggered.
            // Also they are allowed only if sender is Admin.
            if (!commonModule()->resourceAccessManager()->hasGlobalPermission(
                sender->getUserAccessData(), GlobalPermission::admin))
            {
                NX_WARNING(QnLog::EC2_TRAN_LOG,
                    lm("Can't handle transaction %1 because of no administrator rights. "
                       "Reopening connection...").arg(ApiCommand::toString(tran.command)));

                sender->setState(QnTransactionTransport::Error);
                return true;
            }

            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            break;
        }
        case ApiCommand::getFullInfo:
            sender->setWriteSync(true);
            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            break;
        default:
            return false; // Not a special case transaction.
    }

    proxyTransaction(tran, transportHeader);
    return true;
}

} // namespace ec2
