#include "provider.h"

#include <nx/fusion/model_functions.h>

#include "../connection_manager.h"
#include "../incoming_transaction_dispatcher.h"
#include "../outgoing_transaction_dispatcher.h"

namespace nx::clusterdb::engine::statistics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CommandStatistics)(Statistics),
    (json),
    _data_sync_engine_Fields)

//-------------------------------------------------------------------------------------------------

Provider::Provider(
    const ConnectionManager& connectionManager,
    IncomingCommandDispatcher* incomingCommandDispatcher,
    OutgoingCommandDispatcher* outgoingTransactionDispatcher)
    :
    m_connectionManager(connectionManager),
    m_incomingTransactionDispatcher(incomingCommandDispatcher),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_incomingTransactionPerMinuteCalculator(std::chrono::minutes(1)),
    m_outgoingTransactionPerMinuteCalculator(std::chrono::minutes(1))
{
    using namespace std::placeholders;

    m_incomingTransactionDispatcher->watchCommandSubscription().subscribe(
        std::bind(&Provider::updateIncomingTransactionStatistics, this),
        &m_incomingTransactionSubscriptionId);

    m_outgoingTransactionDispatcher->onNewTransactionSubscription().subscribe(
        std::bind(&Provider::updateOutgoingTransactionStatistics, this),
        &m_outgoingTransactionSubscriptionId);
}

Provider::~Provider()
{
    m_outgoingTransactionDispatcher->onNewTransactionSubscription()
        .removeSubscription(m_outgoingTransactionSubscriptionId);

    m_incomingTransactionDispatcher->watchCommandSubscription()
        .removeSubscription(m_incomingTransactionSubscriptionId);
}

Statistics Provider::statistics() const
{
    const auto connections = m_connectionManager.getConnections();

    Statistics data;
    data.connectedServerCount = static_cast<int>(connections.size());
    for (const auto& connection: connections)
        ++data.connectedServerCountByVersion[connection.userAgent];

    QnMutexLocker locker(&m_mutex);

    data.incomingCommands.commandsProcessedPerMinute =
        m_incomingTransactionPerMinuteCalculator.getSumPerLastPeriod();
    data.outgoingCommands.commandsProcessedPerMinute =
        m_outgoingTransactionPerMinuteCalculator.getSumPerLastPeriod();

    return data;
}

void Provider::updateIncomingTransactionStatistics()
{
    QnMutexLocker locker(&m_mutex);
    m_incomingTransactionPerMinuteCalculator.add(1);
}

void Provider::updateOutgoingTransactionStatistics()
{
    QnMutexLocker locker(&m_mutex);
    m_outgoingTransactionPerMinuteCalculator.add(1);
}

} // namespace nx::clusterdb::engine::statistics
