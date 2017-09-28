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
    boost::optional<::ec2::QnTranState> from,
    boost::optional<::ec2::QnTranState> to,
    int maxTransactionsToReturn,
    TransactionsReadHandler completionHandler)
{
    m_transactionLog->readTransactions(
        m_systemId,
        std::move(from),
        std::move(to),
        maxTransactionsToReturn,
        [this,
            sharedGuard = m_asyncOperationGuard.sharedGuard(),
            completionHandler = std::move(completionHandler)](
                api::ResultCode resultCode,
                std::vector<dao::TransactionLogRecord> serializedTransactions,
                ::ec2::QnTranState readedUpTo) mutable
        {
            const auto locker = sharedGuard->lock();
            if (!locker)
                return; //operation has been cancelled

            QnMutexLocker lk(&m_mutex);
            if (m_terminated)
                return;
            post(
                [this, resultCode,
                    serializedTransactions = std::move(serializedTransactions),
                    completionHandler = std::move(completionHandler),
                    readedUpTo = std::move(readedUpTo)]() mutable
                {
                    onTransactionsRead(
                        resultCode,
                        std::move(serializedTransactions),
                        std::move(readedUpTo),
                        std::move(completionHandler));
                });
        });
}

void TransactionLogReader::onTransactionsRead(
    api::ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    ::ec2::QnTranState readedUpTo,
    TransactionsReadHandler completionHandler)
{
    NX_ASSERT(m_dataFormat == Qn::UbjsonFormat);
    // TODO: #ak Converting transaction to JSON format if needed

    completionHandler(
        resultCode,
        std::move(serializedTransactions),
        std::move(readedUpTo));
}

::ec2::QnTranState TransactionLogReader::getCurrentState() const
{
    return m_transactionLog->getTransactionState(m_systemId);
}

nx::String TransactionLogReader::systemId() const
{
    return m_systemId;
}

} // namespace ec2
} // namespace cdb
} // namespace nx
