#pragma once

#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>

#include "connection_manager.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "transaction_log.h"

namespace nx {
namespace cdb {
namespace ec2 {

class Settings;

class SyncronizationEngine
{
public:
    SyncronizationEngine(
        const QnUuid& moduleGuid,
        const Settings& settings,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager);
    ~SyncronizationEngine();

    OutgoingTransactionDispatcher& outgoingTransactionDispatcher();
    const OutgoingTransactionDispatcher& outgoingTransactionDispatcher() const;

    TransactionLog& transactionLog();
    const TransactionLog& transactionLog() const;

    IncomingTransactionDispatcher& incomingTransactionDispatcher();
    const IncomingTransactionDispatcher& incomingTransactionDispatcher() const;

    ConnectionManager& connectionManager();
    const ConnectionManager& connectionManager() const;

    void subscribeToSystemDeletedNotification(
        nx::utils::Subscription<std::string>& subscription);
    void unsubscribeFromSystemDeletedNotification(
        nx::utils::Subscription<std::string>& subscription);

private:
    OutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    TransactionLog m_transactionLog;
    IncomingTransactionDispatcher m_incomingTransactionDispatcher;
    ConnectionManager m_connectionManager;
    nx::utils::SubscriptionId m_systemDeletedSubscriptionId;
    nx::utils::Counter m_startedAsyncCallsCounter;

    void onSystemDeleted(const std::string& systemId);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
