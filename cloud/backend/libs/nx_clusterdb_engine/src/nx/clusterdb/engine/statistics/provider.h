#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

namespace nx::clusterdb::engine { class ConnectionManager; }
namespace nx::clusterdb::engine { class IncomingCommandDispatcher; }
namespace nx::clusterdb::engine { class OutgoingCommandDispatcher; }

namespace nx::clusterdb::engine::statistics {

struct CommandStatistics
{
    int commandsProcessedPerMinute = 0;
};

#define CommandStatistics_data_sync_engine_Fields (commandsProcessedPerMinute)

NX_DATA_SYNC_ENGINE_API bool deserialize(
    QnJsonContext*, const QJsonValue&, CommandStatistics*);
NX_DATA_SYNC_ENGINE_API void serialize(
    QnJsonContext*, const CommandStatistics&, class QJsonValue*);

struct Statistics
{
    int connectedServerCount = 0;
    std::map<std::string, int> connectedServerCountByVersion;
    CommandStatistics incomingCommands;
    CommandStatistics outgoingCommands;
};

#define Statistics_data_sync_engine_Fields \
    (connectedServerCount)(connectedServerCountByVersion)(incomingCommands)(outgoingCommands)

NX_DATA_SYNC_ENGINE_API bool deserialize(
    QnJsonContext*, const QJsonValue&, Statistics*);
NX_DATA_SYNC_ENGINE_API void serialize(
    QnJsonContext*, const Statistics&, class QJsonValue*);

//-------------------------------------------------------------------------------------------------

class NX_DATA_SYNC_ENGINE_API Provider
{
public:
    Provider(
        const ConnectionManager& connectionManager,
        IncomingCommandDispatcher* incomingCommandDispatcher,
        OutgoingCommandDispatcher* outgoingTransactionDispatcher);
    ~Provider();

    Statistics statistics() const;

private:
    const ConnectionManager& m_connectionManager;
    mutable QnMutex m_mutex;

    IncomingCommandDispatcher* m_incomingTransactionDispatcher;
    nx::utils::SubscriptionId m_incomingTransactionSubscriptionId =
        nx::utils::kInvalidSubscriptionId;

    OutgoingCommandDispatcher* m_outgoingTransactionDispatcher;
    nx::utils::SubscriptionId m_outgoingTransactionSubscriptionId =
        nx::utils::kInvalidSubscriptionId;

    nx::utils::math::SumPerPeriod<int> m_incomingTransactionPerMinuteCalculator;
    nx::utils::math::SumPerPeriod<int> m_outgoingTransactionPerMinuteCalculator;

    void updateIncomingTransactionStatistics();
    void updateOutgoingTransactionStatistics();
};

} // namespace nx::clusterdb::engine::statistics
