#include <nx/p2p/p2p_connection_base.h>
#include <nx/p2p/p2p_serialization.h>
#include <utils/common/warnings.h>
#include <transaction/transaction_message_bus_base.h>

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

} // namespace ec2
