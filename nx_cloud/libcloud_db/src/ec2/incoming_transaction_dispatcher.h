#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>
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

/**
 * Dispaches transaction received from remote peer to a corresponding processor.
 */
class IncomingTransactionDispatcher
{
public:
    IncomingTransactionDispatcher(
        const QnUuid& moduleGuid,
        TransactionLog* const transactionLog);

    /** 
     * @note Method is non-blocking, result is delivered by invoking \a completionHandler
     */
    void dispatchTransaction(
        TransactionTransportHeader transportHeader,
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        TransactionProcessedHandler completionHandler);

    /**
     * Register processor function by command type.
     */
    template<int TransactionCommandValue, typename TransactionDataType, typename AuxiliaryArgType>
    void registerTransactionHandler(
        typename TransactionProcessor<
            TransactionCommandValue, TransactionDataType, AuxiliaryArgType
        >::ProcessEc2TransactionFunc processTranFunc,
        typename TransactionProcessor<
            TransactionCommandValue, TransactionDataType, AuxiliaryArgType
        >::OnTranProcessedFunc onTranProcessedFunc)
    {
        m_transactionProcessors.emplace(
            (::ec2::ApiCommand::Value)TransactionCommandValue,
            std::make_unique<TransactionProcessor<
                TransactionCommandValue, TransactionDataType, AuxiliaryArgType>>(
                    m_transactionLog,
                    std::move(processTranFunc),
                    std::move(onTranProcessedFunc)));
    }

    /**
     * Register processor function that does not need to save data to DB.
     */
    template<int TransactionCommandValue, typename TransactionDataType>
    void registerSpecialCommandHandler(
        typename SpecialCommandProcessor<
            TransactionCommandValue, TransactionDataType
        >::ProcessorFunc processTranFunc)
    {
        m_transactionProcessors.emplace(
            (::ec2::ApiCommand::Value)TransactionCommandValue,
            std::make_unique<SpecialCommandProcessor<
                TransactionCommandValue, TransactionDataType>>(
                    std::move(processTranFunc)));
    }

private:
    const QnUuid m_moduleGuid;
    TransactionLog* const m_transactionLog;
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
        if (transaction.command == ::ec2::ApiCommand::updatePersistentSequence)
            return; // TODO: #ak Do something.
        if (it == m_transactionProcessors.end())
        {
            NX_LOGX(lm("Received unsupported transaction %1")
                .arg(::ec2::ApiCommand::toString(transaction.command)), cl_logDEBUG2);
            // No handler registered for transaction type.
            m_aioTimer.post(
                [completionHandler = std::move(completionHandler)]
                {
                    completionHandler(api::ResultCode::notFound);
                });
            return;
        }
        // TODO: should we always call completionHandler in the same thread?
        return it->second->processTransaction(
            transportHeader,
            transaction,
            dataSource,
            std::move(completionHandler));
    }
};

} // namespace ec2
} // namespace cdb
} // namespace nx
