#include "transaction_log_reader.h"

#include "transaction_log.h"

namespace nx {
namespace cdb {
namespace ec2 {

TransactionLogReader::TransactionLogReader(
    TransactionLog* const transactionLog,
    nx::String systemId,
    Qn::SerializationFormat dataFormat)
    :
    m_transactionLog(transactionLog),
    m_systemId(systemId),
    m_dataFormat(dataFormat),
    m_terminated(false)
{
}

TransactionLogReader::~TransactionLogReader()
{
    stopWhileInAioThread();
}

void TransactionLogReader::stopWhileInAioThread()
{
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
    }
    m_asyncOperationGuard.reset();
}

void TransactionLogReader::readTransactions(
    const ::ec2::QnTranState& from,
    const ::ec2::QnTranState& to,
    int maxTransactionsToReturn,
    nx::utils::MoveOnlyFunc<void(
        api::ResultCode /*resultCode*/,
        std::vector<nx::Buffer> /*serializedTransactions*/)> completionHandler)
{
    m_transactionLog->readTransactions(
        m_systemId,
        from,
        to,
        maxTransactionsToReturn,
        [this, 
            sharedGuard = m_asyncOperationGuard.sharedGuard(),
            completionHandler = std::move(completionHandler)](
                api::ResultCode resultCode,
                std::vector<nx::Buffer> serializedTransactions) mutable
        {
            const auto locker = sharedGuard->lock();
            if (!locker)
                return; //operation has been cancelled

            QnMutexLocker lk(&m_mutex);
            if (m_terminated)
                return;
            post(
                [resultCode,
                    serializedTransactions = std::move(serializedTransactions),
                    completionHandler = std::move(completionHandler)]() mutable
                {
                    completionHandler(resultCode, std::move(serializedTransactions));
                });
        });
}

::ec2::QnTranState TransactionLogReader::getCurrentState() const
{
    return m_transactionLog->getTransactionState(m_systemId);
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
