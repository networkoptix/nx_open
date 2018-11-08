#pragma once

#include <memory>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/counter.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sql/async_sql_query_executor.h>

#include "connection_statistics_info.h"
#include "dao/abstract_data_object.h"
#include "../settings.h"

namespace nx {
namespace hpm {
namespace stats {

class AbstractCollector
{
public:
    virtual ~AbstractCollector() = default;

    virtual void saveConnectSessionStatistics(const ConnectSession& data) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Saves session information to DB.
 */
class PersistentCollector:
    public AbstractCollector
{
public:
    PersistentCollector(
        const conf::Statistics& settings,
        nx::sql::AsyncSqlQueryExecutor* sqlQueryExecutor);
    virtual ~PersistentCollector() override;

    virtual void saveConnectSessionStatistics(const ConnectSession& data) override;

private:
    const conf::Statistics m_settings;
    nx::sql::AsyncSqlQueryExecutor* m_sqlQueryExecutor;
    std::unique_ptr<dao::AbstractDataObject> m_dataObject;
    nx::utils::Counter m_startedAsyncCallsCounter;
};

//-------------------------------------------------------------------------------------------------

struct CloudConnectStatistics
{
    int serverCount = 0;
    int clientCount = 0;
    int connectionsEstablishedPerMinute = 0;
    int connectionsFailedPerMinute = 0;
};

#define CloudConnectStatistics_Fields (serverCount)(clientCount) \
    (connectionsEstablishedPerMinute)(connectionsFailedPerMinute)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (CloudConnectStatistics),
    (json))

/**
 * Calculates some statistics using session data.
 */
class StatisticsCalculator:
    public AbstractCollector
{
public:
    StatisticsCalculator();

    virtual void saveConnectSessionStatistics(const ConnectSession& data) override;

    CloudConnectStatistics statistics() const;

private:
    mutable QnMutex m_mutex;
    nx::utils::math::SumPerPeriod<int> m_connectionsEstablishedPerPeriod;
    nx::utils::math::SumPerPeriod<int> m_connectionsFailedPerPeriod;
};

//-------------------------------------------------------------------------------------------------

/**
 * Just forwardes session information to multiple statistics collectors.
 */
class StatisticsForwarder:
    public AbstractCollector
{
public:
    StatisticsForwarder(std::vector<std::unique_ptr<AbstractCollector>> collectors);

    virtual void saveConnectSessionStatistics(const ConnectSession& data) override;

private:
    std::vector<std::unique_ptr<AbstractCollector>> m_collectors;
};

//-------------------------------------------------------------------------------------------------

using CollectorFactoryFunction =
    std::unique_ptr<AbstractCollector>(
        const conf::Statistics& settings,
        nx::sql::AsyncSqlQueryExecutor* sqlQueryExecutor);

class CollectorFactory:
    public nx::utils::BasicFactory<CollectorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<CollectorFactoryFunction>;

public:
    CollectorFactory();

    static CollectorFactory& instance();

private:
    std::unique_ptr<AbstractCollector> defaultFactoryFunction(
        const conf::Statistics& settings,
        nx::sql::AsyncSqlQueryExecutor* sqlQueryExecutor);
};

} // namespace stats
} // namespace hpm
} // namespace nx
