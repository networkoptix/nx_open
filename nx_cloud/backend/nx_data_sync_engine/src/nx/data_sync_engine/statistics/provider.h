#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

namespace nx::data_sync_engine { class ConnectionManager; }
namespace nx::data_sync_engine { class IncomingTransactionDispatcher; }
namespace nx::data_sync_engine { class OutgoingTransactionDispatcher; }

namespace nx::data_sync_engine::statistics {

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
    std::map<std::string, int> connectedServerCountByVersion;
    CommandStatistics incomingCommands;
    CommandStatistics outgoingCommands;
};

#define Statistics_data_sync_engine_Fields \
    (connectedServerCountByVersion)(incomingCommands)(outgoingCommands)

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
        IncomingTransactionDispatcher* incomingTransactionDispatcher,
        OutgoingTransactionDispatcher* outgoingTransactionDispatcher);
    ~Provider();

    Statistics statistics() const;

private:
    const ConnectionManager& m_connectionManager;
    mutable QnMutex m_mutex;

    IncomingTransactionDispatcher* m_incomingTransactionDispatcher;
    nx::utils::SubscriptionId m_incomingTransactionSubscriptionId =
        nx::utils::kInvalidSubscriptionId;

    OutgoingTransactionDispatcher* m_outgoingTransactionDispatcher;
    nx::utils::SubscriptionId m_outgoingTransactionSubscriptionId =
        nx::utils::kInvalidSubscriptionId;

    nx::utils::math::SumPerPeriod<int> m_incomingTransactionPerMinuteCalculator;
    nx::utils::math::SumPerPeriod<int> m_outgoingTransactionPerMinuteCalculator;

    void updateIncomingTransactionStatistics();
    void updateOutgoingTransactionStatistics();
};

} // namespace nx::data_sync_engine::statistics
