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
    const nx::String& /*systemId*/,
    nx::utils::MoveOnlyFunc<nx::db::DBResult(QSqlDatabase*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, nx::db::DBResult)> onDbUpdateCompleted)
{
    // TODO: execution of requests to the same system MUST be serialized
    // TODO: monitoring request queue size and returning api::ResultCode::retryLater if exceeded

    m_dbManager->executeUpdate(
        [dbOperationsFunc = std::move(dbOperationsFunc)](
            QSqlDatabase* dbConnection) -> nx::db::DBResult
        {
            return dbOperationsFunc(dbConnection);
        },
        [this, onDbUpdateCompleted = std::move(onDbUpdateCompleted)](
            QSqlDatabase* dbConnection,
            nx::db::DBResult dbResult)
        {
            if (dbResult != nx::db::DBResult::ok)
            {
                // TODO: #ak rolling back transaction log change
            }
            onDbUpdateCompleted(dbConnection, dbResult);

            // TODO: #ak issuing "new transaction" event
            QnMutexLocker lk(&m_mutex);
            m_dbTransactionContexts.erase(dbConnection);
        });
}

////////////////////////////////////////////////////////////
//// class TransactionLogReader
////////////////////////////////////////////////////////////

TransactionLogReader::TransactionLogReader(
    TransactionLog* const transactionLog,
    nx::String systemId,
    Qn::SerializationFormat dataFormat)
{
    // TODO:
}

TransactionLogReader::~TransactionLogReader()
{
    stopWhileInAioThread();
}

void TransactionLogReader::stopWhileInAioThread()
{
    // TODO:
}

void TransactionLogReader::getTransactions(
    const ::ec2::QnTranState& /*from*/,
    const ::ec2::QnTranState& /*to*/,
    int /*maxTransactionsToReturn*/,
    nx::utils::MoveOnlyFunc<void(
        api::ResultCode /*resultCode*/,
        std::vector<nx::Buffer> /*serializedTransactions*/)> completionHandler)
{
    // TODO:
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            completionHandler(api::ResultCode::ok, std::vector<nx::Buffer>());
        });
}

::ec2::QnTranState TransactionLogReader::getCurrentState() const
{
    // TODO:
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

} // namespace ec2
} // namespace cdb
} // namespace nx
