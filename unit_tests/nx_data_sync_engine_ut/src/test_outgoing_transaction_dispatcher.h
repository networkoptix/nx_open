#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <utility>

#include <nx/utils/move_only_func.h>

#include <nx/data_sync_engine/outgoing_transaction_dispatcher.h>

namespace nx {
namespace data_sync_engine {
namespace test {

class TestOutgoingTransactionDispatcher:
    public AbstractOutgoingTransactionDispatcher
{
public:
    typedef nx::utils::MoveOnlyFunc<
        void(const nx::String&, const SerializableAbstractTransaction&)
    > OnNewTransactionHandler;

    virtual void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer) override;

    void setOnNewTransaction(OnNewTransactionHandler onNewTransactionHandler);
    void setDelayBeforeSavingTransaction(
        std::chrono::milliseconds min, std::chrono::milliseconds max);

    void clear();

    //---------------------------------------------------------------------------------------------
    // asserts

    void assertIfTransactionsWereNotSentInAscendingSequenceOrder();

    template<typename TransactionPtrContainer>
    void assertIfNotAllTransactionsAreFound(const TransactionPtrContainer& container)
    {
        for (const auto& transactionPtr: container)
            assertIfCouldNotFindTransactionWithHeader(transactionPtr->transactionHeader());
    }

    void assertIfCouldNotFindTransactionWithHeader(
        const CommandHeader& transactionHeader);

private:
    OnNewTransactionHandler m_onNewTransactionHandler;
    QnMutex m_mutex;
    std::list<std::shared_ptr<const SerializableAbstractTransaction>> m_outgoingTransactions;
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> m_delayBeforeSavingTransaction;
};

} // namespace test
} // namespace data_sync_engine
} // namespace nx
