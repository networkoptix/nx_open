#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx_ec/data/api_tran_state_data.h>

#include <nx/cloud/cdb/api/result_code.h>

#include "transaction_log.h"

namespace nx {
namespace cdb {
namespace ec2 {

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
        boost::optional<::ec2::QnTranState> from,
        boost::optional<::ec2::QnTranState> to,
        int maxTransactionsToReturn,
        TransactionsReadHandler completionHandler);

    // TODO: #ak following method MUST be asynchronous
    ::ec2::QnTranState getCurrentState() const;
    nx::String systemId() const;

private:
    TransactionLog* const m_transactionLog;
    const nx::String m_systemId;
    const Qn::SerializationFormat m_dataFormat;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    bool m_terminated;
    QnMutex m_mutex;

    void onTransactionsRead(
        api::ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        ::ec2::QnTranState readedUpTo,
        TransactionsReadHandler completionHandler);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
