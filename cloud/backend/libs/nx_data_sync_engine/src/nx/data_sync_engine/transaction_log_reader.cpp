#include "transaction_log_reader.h"

#include "outgoing_command_filter.h"
#include "transaction_log.h"

namespace nx::data_sync_engine {

TransactionLogReader::TransactionLogReader(
    TransactionLog* const transactionLog,
    const std::string& systemId,
    Qn::SerializationFormat dataFormat,
    const OutgoingCommandFilter& outgoingCommandFilter)
    :
    m_transactionLog(transactionLog),
    m_systemId(systemId),
    m_dataFormat(dataFormat),
    m_outgoingCommandFilter(outgoingCommandFilter),
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
    const ReadCommandsFilter& readFilter,
    TransactionsReadHandler completionHandler)
{
    ReadCommandsFilter effectiveReadFilter = readFilter;
    m_outgoingCommandFilter.updateReadFilter(&effectiveReadFilter);

    m_transactionLog->readTransactions(
        m_systemId,
        effectiveReadFilter,
        [this,
            sharedGuard = m_asyncOperationGuard.sharedGuard(),
            completionHandler = std::move(completionHandler)](
                ResultCode resultCode,
                std::vector<dao::TransactionLogRecord> serializedTransactions,
                vms::api::TranState readedUpTo) mutable
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
    ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState readedUpTo,
    TransactionsReadHandler completionHandler)
{
    NX_ASSERT(m_dataFormat == Qn::UbjsonFormat);
    // TODO: #ak Converting transaction to JSON format if needed

    completionHandler(
        resultCode,
        std::move(serializedTransactions),
        std::move(readedUpTo));
}

vms::api::TranState TransactionLogReader::getCurrentState() const
{
    return m_transactionLog->getTransactionState(m_systemId);
}

std::string TransactionLogReader::systemId() const
{
    return m_systemId;
}

} // namespace nx::data_sync_engine
