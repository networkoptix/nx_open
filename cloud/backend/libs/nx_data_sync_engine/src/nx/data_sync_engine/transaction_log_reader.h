#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/tran_state_data.h>

#include "transaction_log.h"

namespace nx::clusterdb::engine {

class OutgoingCommandFilter;

/**
 * Asynchronously reads transactions of specified system from log.
 * Returns data in specified format.
 */
class TransactionLogReader:
    public network::aio::BasicPollable
{
public:
    typedef TransactionLog::TransactionsReadHandler TransactionsReadHandler;

    TransactionLogReader(
        TransactionLog* const transactionLog,
        const std::string& systemId,
        Qn::SerializationFormat dataFormat,
        const OutgoingCommandFilter& outgoingCommandFilter);
    ~TransactionLogReader();

    virtual void stopWhileInAioThread() override;

    void readTransactions(
        const ReadCommandsFilter& commandFilter,
        TransactionsReadHandler completionHandler);

    // TODO: #ak following method MUST be asynchronous
    vms::api::TranState getCurrentState() const;
    std::string systemId() const;

private:
    TransactionLog* const m_transactionLog;
    const std::string m_systemId;
    const Qn::SerializationFormat m_dataFormat;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    bool m_terminated;
    QnMutex m_mutex;

    void onTransactionsRead(
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo,
        TransactionsReadHandler completionHandler);
};

} // namespace nx::clusterdb::engine
