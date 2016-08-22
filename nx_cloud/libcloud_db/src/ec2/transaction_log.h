/**********************************************************
* Aug 16, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>

#include <cdb/result_code.h>
#include <utils/db/async_sql_query_executor.h>

#include <transaction/transaction.h>

#include "transaction_transport_header.h"


namespace nx {

namespace db {
class AsyncSqlQueryExecutor;
}   // namespace db

namespace cdb {
namespace ec2 {

/**  */
class TransactionLog
{
public:
    TransactionLog(nx::db::AsyncSqlQueryExecutor* const dbManager);

    /** Begins SQL DB transaction and passes that to \a dbOperationsFunc.
        \note api::ResultCode::retryLater can be returned if 
            there are already too many requests for transaction
    */
    void startDbTransaction(
        const nx::String& systemId,
        nx::utils::MoveOnlyFunc<nx::db::DBResult(api::ResultCode, QSqlDatabase*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::DBResult)> onDbUpdateCompleted);

    /** 
        If transaction is not needed (it can be late or something), 
            \a db::DBResult::cancelled is returned
    */
    template<typename TransactionDataType>
    nx::db::DBResult checkIfNeededAndSaveToLog(
        QSqlDatabase* /*connection*/,
        const nx::String& /*systemId*/,
        ::ec2::QnTransaction<TransactionDataType> /*transaction*/,
        TransactionTransportHeader /*transportHeader*/)
    {
        //TODO
        return db::DBResult::statementError;
    }

private:
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
};

/** Asynchronously reads transactions of specified system from log.
    Returns data in specified format
*/
class TransactionLogReader
:
    public network::aio::BasicPollable
{
public:
    TransactionLogReader(
        TransactionLog* const transactionLog,
        nx::Buffer systemId,
        Qn::SerializationFormat dataFormat);
    ~TransactionLogReader();

    virtual void stopWhileInAioThread() override;

    void getTransactions(
        const ::ec2::QnTranState& from,
        const ::ec2::QnTranState& to,
        int maxTransactionsToReturn,
        nx::utils::MoveOnlyFunc<void(
            api::ResultCode resultCode,
            std::vector<nx::Buffer> serializedTransactions)> completionHandler);
    ::ec2::QnTranState getCurrentState() const;

    /** Called before returning ubjson-transaction to the caller.
        Handler is allowed to modify transaction. E.g., add transport header
    */
    void setOnUbjsonTransactionReady(
        nx::utils::MoveOnlyFunc<void(nx::Buffer)> handler);
    /** Called before returning JSON-transaction to the caller.
        Handler is allowed to modify transaction. E.g., add transport header
    */
    void setOnJsonTransactionReady(
        nx::utils::MoveOnlyFunc<void(QJsonObject*)> handler);

private:
    nx::utils::MoveOnlyFunc<void(nx::Buffer)> m_onUbjsonTransactionReady;
    nx::utils::MoveOnlyFunc<void(QJsonObject*)> m_onJsonTransactionReady;
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
