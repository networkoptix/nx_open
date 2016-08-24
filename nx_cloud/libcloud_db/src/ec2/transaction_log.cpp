/**********************************************************
* Aug 22, 2016
* a.kolesnikov
***********************************************************/

#include "transaction_log.h"


namespace nx {
namespace cdb {
namespace ec2 {

////////////////////////////////////////////////////////////
//// class TransactionLog
////////////////////////////////////////////////////////////

TransactionLog::TransactionLog(nx::db::AsyncSqlQueryExecutor* const dbManager)
:
    m_dbManager(dbManager)
{
}

void TransactionLog::startDbTransaction(
    const nx::String& systemId,
    nx::utils::MoveOnlyFunc<nx::db::DBResult(api::ResultCode, QSqlDatabase*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(nx::db::DBResult)> onDbUpdateCompleted)
{
    //TODO execution of requests to same system MUST be serialized
    //TODO monitoring request queue size and returning api::ResultCode::retryLater if exceeded

    //TODO
}

////////////////////////////////////////////////////////////
//// class TransactionLogReader
////////////////////////////////////////////////////////////

TransactionLogReader::TransactionLogReader(
    TransactionLog* const transactionLog,
    nx::Buffer systemId,
    Qn::SerializationFormat dataFormat)
{
    //TODO
}

TransactionLogReader::~TransactionLogReader()
{
    stopWhileInAioThread();
}

void TransactionLogReader::stopWhileInAioThread()
{
    //TODO
}

void TransactionLogReader::getTransactions(
    const ::ec2::QnTranState& /*from*/,
    const ::ec2::QnTranState& /*to*/,
    int /*maxTransactionsToReturn*/,
    nx::utils::MoveOnlyFunc<void(
        api::ResultCode /*resultCode*/,
        std::vector<nx::Buffer> /*serializedTransactions*/)> completionHandler)
{
    //TODO
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            completionHandler(api::ResultCode::dbError, std::vector<nx::Buffer>());
        });
}

::ec2::QnTranState TransactionLogReader::getCurrentState() const
{
    //TODO
    return ::ec2::QnTranState();
}

void TransactionLogReader::setOnUbjsonTransactionReady(
    nx::utils::MoveOnlyFunc<void(nx::Buffer)> handler)
{
    m_onUbjsonTransactionReady = std::move(handler);
}

void TransactionLogReader::setOnJsonTransactionReady(
    nx::utils::MoveOnlyFunc<void(QJsonObject*)> handler)
{
    m_onJsonTransactionReady = std::move(handler);
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
