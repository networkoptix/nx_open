/**********************************************************
* Aug 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/utils/move_only_func.h>

#include <common/common_globals.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction.h>

#include "transaction_processor.h"


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

/** Does abstract transaction processing logic.
    Specific transaction logic is implemented by specific manager
*/
class TransactionDispatcher
{
public:
    TransactionDispatcher(TransactionLog* const transactionLog);

    /** 
        @return \a false, if could not parse transaction or no handler installed for such transaction type
    */
    bool processTransaction(
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        const ::ec2::QnTransactionTransportHeader& transportHeader);

    template<int TransactionCommandValue, typename TransactionDataType>
    void registerTransactionHandler(
        nx::utils::MoveOnlyFunc<void(TransactionDataType)> processTranFunc)
    {
        m_transactionProcessors.emplace(
            (::ec2::ApiCommand::Value)TransactionCommandValue,
            std::make_unique<typename TransactionProcessor<
                TransactionCommandValue, TransactionDataType>>(
                    m_transactionLog,
                    std::move(processTranFunc)));
    }

private:
    TransactionLog* const m_transactionLog;
    std::map<
        ::ec2::ApiCommand::Value,
        std::unique_ptr<AbstractTransactionProcessor>
    > m_transactionProcessors;

    template<typename TransactionDataSource>
    bool dispatchTransaction(
        const ::ec2::QnTransactionTransportHeader& transportHeader,
        const ::ec2::QnAbstractTransaction& transaction,
        const TransactionDataSource& dataSource)
    {
        auto it = m_transactionProcessors.find(transaction.command);
        if (it == m_transactionProcessors.end())
            return false;   //no handler registered for transaction type
        return it->second->processTransaction(transportHeader, transaction, dataSource);
    }
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
