/**********************************************************
* Aug 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>
#include <utils/db/async_sql_query_executor.h>

#include <cdb/result_code.h>
#include <common/common_globals.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction.h>

#include "transaction_processor.h"
#include "transaction_transport_header.h"


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

/** Dispaches transaction received from remote peer to a corresponding processor. */
class IncomingTransactionDispatcher
{
public:
    IncomingTransactionDispatcher(
        TransactionLog* const transactionLog,
        nx::db::AsyncSqlQueryExecutor* const dbManager);

    /** 
        \note Method is non-blocking, result is delivered by invoking \a completionHandler
    */
    void dispatchTransaction(
        TransactionTransportHeader transportHeader,
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        TransactionProcessedHandler completionHandler);

    /** register processor function by command type */
    template<int TransactionCommandValue, typename TransactionDataType>
    void registerTransactionHandler(
        typename TransactionProcessor<
            TransactionCommandValue, TransactionDataType
        >::ProcessEc2TransactionFunc processTranFunc)
    {
        m_transactionProcessors.emplace(
            (::ec2::ApiCommand::Value)TransactionCommandValue,
            std::make_unique<typename TransactionProcessor<
                TransactionCommandValue, TransactionDataType>>(
                    m_transactionLog,
                    m_dbManager,
                    std::move(processTranFunc)));
    }

    /** register processor function that will do everything itself */
    template<int TransactionCommandValue, typename TransactionDataType>
    void registerSpecialTransactionHandler(
        nx::utils::MoveOnlyFunc<void(
            const nx::String& /*systemId*/,
            const TransactionTransportHeader& /*transportHeader*/,
            TransactionDataType /*data*/,
            TransactionProcessedHandler /*handler*/)> /*processTranFunc*/)
    {
        //TODO #ak
    }

private:
    TransactionLog* const m_transactionLog;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    std::map<
        ::ec2::ApiCommand::Value,
        std::unique_ptr<AbstractTransactionProcessor>
    > m_transactionProcessors;
    nx::network::aio::Timer m_aioTimer;

    template<typename TransactionDataSource>
    void dispatchTransaction(
        const TransactionTransportHeader& transportHeader,
        const ::ec2::QnAbstractTransaction& transaction,
        const TransactionDataSource& dataSource,
        TransactionProcessedHandler completionHandler)
    {
        auto it = m_transactionProcessors.find(transaction.command);
        if (it == m_transactionProcessors.end())
        {
            //no handler registered for transaction type
            m_aioTimer.post(
                [completionHandler = std::move(completionHandler)]
                {
                    completionHandler(api::ResultCode::badRequest);
                });
            return;
        }
        return it->second->processTransaction(
            transportHeader,
            transaction,
            dataSource,
            std::move(completionHandler));
    }
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
