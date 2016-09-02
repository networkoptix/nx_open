#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx_ec/data/api_tran_state_data.h>

#include <cdb/result_code.h>

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

/**
 * Asynchronously reads transactions of specified system from log.
 * Returns data in specified format.
 */
class TransactionLogReader
    :
    public network::aio::BasicPollable
{
public:
    TransactionLogReader(
        TransactionLog* const transactionLog,
        nx::String systemId,
        Qn::SerializationFormat dataFormat);
    ~TransactionLogReader();

    virtual void stopWhileInAioThread() override;

    void readTransactions(
        const ::ec2::QnTranState& from,
        const ::ec2::QnTranState& to,
        int maxTransactionsToReturn,
        nx::utils::MoveOnlyFunc<void(
            api::ResultCode resultCode,
            std::vector<nx::Buffer> serializedTransactions)> completionHandler);

    // TODO: #ak following method MUST be asynchronous
    ::ec2::QnTranState getCurrentState() const;

    /**
     * Called before returning ubjson-transaction to the caller.
     * Handler is allowed to modify transaction. E.g., add transport header
     */
    void setOnUbjsonTransactionReady(
        nx::utils::MoveOnlyFunc<void(nx::Buffer)> handler);
    /**
     * Called before returning JSON-transaction to the caller.
     * Handler is allowed to modify transaction. E.g., add transport header
     */
    void setOnJsonTransactionReady(
        nx::utils::MoveOnlyFunc<void(QJsonObject*)> handler);

private:
    TransactionLog* const m_transactionLog;
    const nx::String m_systemId;
    const Qn::SerializationFormat m_dataFormat;
    nx::utils::MoveOnlyFunc<void(nx::Buffer)> m_onUbjsonTransactionReady;
    nx::utils::MoveOnlyFunc<void(QJsonObject*)> m_onJsonTransactionReady;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    bool m_terminated;
    QnMutex m_mutex;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
