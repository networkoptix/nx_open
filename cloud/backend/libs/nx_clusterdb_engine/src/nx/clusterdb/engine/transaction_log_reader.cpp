#include "transaction_log_reader.h"

#include <nx/utils/log/log.h>

#include "outgoing_command_filter.h"
#include "transaction_log.h"

namespace nx::clusterdb::engine {

CommandLogReader::CommandLogReader(
    CommandLog* const commandLog,
    const std::string& systemId,
    Qn::SerializationFormat dataFormat,
    const OutgoingCommandFilter& outgoingCommandFilter)
    :
    m_commandLog(commandLog),
    m_systemId(systemId),
    m_dataFormat(dataFormat),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_terminated(false)
{
}

CommandLogReader::~CommandLogReader()
{
    stopWhileInAioThread();
}

void CommandLogReader::stopWhileInAioThread()
{
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
    }
    m_asyncOperationGuard.reset();
}

void CommandLogReader::readTransactions(
    const ReadCommandsFilter& readFilter,
    TransactionsReadHandler completionHandler)
{
    NX_DEBUG(this,
        lm("systemId %1. Reading commands from (%2) to (%3)")
        .args(m_systemId, readFilter.from ? stateToString(*readFilter.from) : "none",
            readFilter.to ? stateToString(*readFilter.to) : "none"));

    ReadCommandsFilter effectiveReadFilter = readFilter;
    m_outgoingCommandFilter.updateReadFilter(&effectiveReadFilter);

    m_commandLog->readTransactions(
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

void CommandLogReader::onTransactionsRead(
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

vms::api::TranState CommandLogReader::getCurrentState() const
{
    return m_commandLog->getTransactionState(m_systemId);
}

std::string CommandLogReader::systemId() const
{
    return m_systemId;
}

//-------------------------------------------------------------------------------------------------

std::string stateToString(const vms::api::TranState& tranState)
{
    std::string str;
    for (auto it = tranState.values.begin(); it != tranState.values.end(); ++it)
    {
        if (!str.empty())
            str += ", ";

        str += "{" + it.key().id.toSimpleString().toStdString() + ", " +
            it.key().persistentId.toSimpleString().toStdString() + "}";
        str += ": ";
        str += std::to_string(it.value());
    }

    return str;
}

} // namespace nx::clusterdb::engine
