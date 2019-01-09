#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <utility>

#include <nx/utils/move_only_func.h>

#include <nx/clusterdb/engine/outgoing_transaction_dispatcher.h>

namespace nx::clusterdb::engine {
namespace test {

class TestOutgoingTransactionDispatcher:
    public AbstractOutgoingCommandDispatcher
{
public:
    typedef nx::utils::MoveOnlyFunc<
        void(const std::string&, const SerializableAbstractCommand&)
    > OnNewTransactionHandler;

    virtual void dispatchTransaction(
        const std::string& systemId,
        std::shared_ptr<const SerializableAbstractCommand> transactionSerializer) override;

    void setOnNewTransaction(OnNewTransactionHandler onNewTransactionHandler);
    void setDelayBeforeSavingTransaction(
        std::chrono::milliseconds min, std::chrono::milliseconds max);

    void clear();

    //---------------------------------------------------------------------------------------------
    // asserts

    void assertTransactionsAreSentInAscendingSequenceOrder();

    template<typename TransactionPtrContainer>
    void assertAllTransactionsAreFound(const TransactionPtrContainer& container)
    {
        for (const auto& transactionPtr: container)
            assertTransactionWithHeaderIsFound(transactionPtr->header());
    }

    void assertTransactionWithHeaderIsFound(
        const CommandHeader& header);

private:
    OnNewTransactionHandler m_onNewTransactionHandler;
    QnMutex m_mutex;
    std::list<std::shared_ptr<const SerializableAbstractCommand>> m_outgoingTransactions;
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> m_delayBeforeSavingTransaction;
};

} // namespace test
} // namespace nx::clusterdb::engine
