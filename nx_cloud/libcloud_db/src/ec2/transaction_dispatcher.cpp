/**********************************************************
* Aug 12, 2016
* a.kolesnikov
***********************************************************/

#include "transaction_dispatcher.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>
#include <transaction/transaction.h>


namespace nx {
namespace cdb {
namespace ec2 {

using namespace ::ec2;

TransactionDispatcher::TransactionDispatcher(
    TransactionLog* const transactionLog,
    nx::db::AsyncSqlQueryExecutor* const dbManager)
:
    m_transactionLog(transactionLog),
    m_dbManager(dbManager)
{
    registerTransactionHandler<::ec2::ApiCommand::saveUser, ::ec2::ApiUserData>(nullptr);
}

void TransactionDispatcher::dispatchTransaction(
    TransactionTransportHeader transportHeader,
    Qn::SerializationFormat tranFormat,
    const QByteArray& serializedTransaction,
    TransactionProcessedHandler handler)
{
    if (tranFormat == Qn::UbjsonFormat)
    {
        QnAbstractTransaction transaction;
        QnUbjsonReader<QByteArray> stream(&serializedTransaction);
        if (!QnUbjson::deserialize(&stream, &transaction))
        {
            NX_LOGX(lm("Failed to deserialized ubjson transaction received from (%1, %2). size %3")
                .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
                .arg(serializedTransaction.size()), cl_logDEBUG1);
            m_aioTimer.post(
                [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
            return;
        }

        return dispatchTransaction(
            std::move(transportHeader),
            std::move(transaction),
            &stream,
            std::move(handler));
    }
    else if (tranFormat == Qn::JsonFormat)
    {
        QnAbstractTransaction transaction;
        QJsonObject tranObject;
        //TODO #ak take tranObject from cache
        if (!QJson::deserialize(serializedTransaction, &tranObject))
        {
            NX_LOGX(lm("Failed to parse json transaction received from (%1, %2). size %3")
                .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
                .arg(serializedTransaction.size()), cl_logDEBUG1);
            m_aioTimer.post(
                [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
            return;
        }
        if (!QJson::deserialize(tranObject["tran"], &transaction))
        {
            NX_LOGX(lm("Failed to deserialize json transaction received from (%1, %2). size %3")
                .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
                .arg(serializedTransaction.size()), cl_logDEBUG1);
            m_aioTimer.post(
                [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
            return;
        }

        return dispatchTransaction(
            std::move(transportHeader),
            std::move(transaction),
            tranObject["tran"].toObject(),
            std::move(handler));
    }
    else
    {
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
    }
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
