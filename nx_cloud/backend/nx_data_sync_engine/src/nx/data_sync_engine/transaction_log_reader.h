#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/tran_state_data.h>

#include "transaction_log.h"

namespace nx {
namespace data_sync_engine {

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
        nx::String systemId,
        Qn::SerializationFormat dataFormat);
    ~TransactionLogReader();

    virtual void stopWhileInAioThread() override;

    void readTransactions(
        boost::optional<vms::api::TranState> from,
        boost::optional<vms::api::TranState> to,
        int maxTransactionsToReturn,
        TransactionsReadHandler completionHandler);

    // TODO: #ak following method MUST be asynchronous
    vms::api::TranState getCurrentState() const;
    nx::String systemId() const;

private:
    TransactionLog* const m_transactionLog;
    const nx::String m_systemId;
    const Qn::SerializationFormat m_dataFormat;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    bool m_terminated;
    QnMutex m_mutex;

    void onTransactionsRead(
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo,
        TransactionsReadHandler completionHandler);
};

} // namespace data_sync_engine
} // namespace nx
