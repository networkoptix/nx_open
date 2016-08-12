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

TransactionDispatcher::TransactionDispatcher(TransactionLog* const transactionLog)
:
    m_transactionLog(transactionLog)
{
    registerTransactionHandler<::ec2::ApiCommand::saveUser, ::ec2::ApiUserData>(nullptr);
}

bool TransactionDispatcher::processTransaction(
    Qn::SerializationFormat tranFormat,
    const QByteArray& serializedTransaction,
    const ::ec2::QnTransactionTransportHeader& transportHeader)
{
    if (tranFormat == Qn::UbjsonFormat)
    {
        QnAbstractTransaction transaction;
        QnUbjsonReader<QByteArray> stream(&serializedTransaction);
        if (!QnUbjson::deserialize(&stream, &transaction))
        {
            NX_LOGX(lit("Failed to deserialized ubjson transaction received from TODO. size %1")
                .arg(serializedTransaction.size()), cl_logDEBUG1);
            return false;
        }

        return dispatchTransaction(
            transportHeader,
            transaction,
            &stream);
    }
    else if (tranFormat == Qn::JsonFormat)
    {
        QnAbstractTransaction transaction;
        QJsonObject tranObject;
        //TODO #ak take tranObject from cache
        if (!QJson::deserialize(serializedTransaction, &tranObject))
        {
            NX_LOGX(lit("Failed to parse json transaction received from TODO. size %1")
                .arg(serializedTransaction.size()), cl_logDEBUG1);
            return false;
        }
        if (!QJson::deserialize(tranObject["tran"], &transaction))
        {
            NX_LOGX(lit("Failed to deserialize json transaction received from TODO. size %1")
                .arg(serializedTransaction.size()), cl_logDEBUG1);
            return false;
        }

        return dispatchTransaction(
            transportHeader,
            transaction,
            tranObject["tran"].toObject());
    }
    else
    {
        return false;
    }
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
