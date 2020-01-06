#include "collector.h"

#include <nx/fusion/model_functions.h>

#include "dao/data_object_factory.h"

namespace nx {
namespace hpm {
namespace stats {

PersistentCollector::PersistentCollector(
    const conf::Statistics& settings,
    nx::sql::AbstractAsyncSqlQueryExecutor* sqlQueryExecutor)
    :
    m_settings(settings),
    m_sqlQueryExecutor(sqlQueryExecutor),
    m_dataObject(dao::DataObjectFactory::create())
{
}

PersistentCollector::~PersistentCollector()
{
    m_startedAsyncCallsCounter.wait();
}

void PersistentCollector::saveConnectSessionStatistics(const ConnectSession& data)
{
    using namespace std::placeholders;

    m_sqlQueryExecutor->executeUpdate(
        std::bind(&dao::AbstractDataObject::save, m_dataObject.get(), _1, std::move(data)),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::sql::DBResult /*dbResult*/) {});
}

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CloudConnectStatistics),
    (json),
    _Fields)

void StatisticsCalculator::saveConnectSessionStatistics(const ConnectSession& data)
{
    QnMutexLocker lock(&m_mutex);

    if (data.resultCode == api::NatTraversalResultCode::ok)
    {
        m_connectionsEstablishedPerPeriod.add(1);
        m_establishedConnectionsPerType[data.connectType].add(1);
    }
    else
    {
        m_connectionsFailedPerPeriod.add(1);
        m_failedConnectionsPerResultCode[data.resultCode].add(1);
    }
}

CloudConnectStatistics StatisticsCalculator::statistics() const
{
    QnMutexLocker lock(&m_mutex);

    CloudConnectStatistics statistics;

    statistics.totalConnectionsEstablishedPerMinute =
        m_connectionsEstablishedPerPeriod.getSumPerLastPeriod();

    for (const auto& [connectType, counter]: m_establishedConnectionsPerType)
        statistics.establishedConnectionsPerType[connectType] = counter.getSumPerLastPeriod();

    statistics.totalConnectionsFailedPerMinute =
        m_connectionsFailedPerPeriod.getSumPerLastPeriod();

    for (const auto& [resultCode, counter]: m_failedConnectionsPerResultCode)
        statistics.failedConnectionsPerResultCode[resultCode] = counter.getSumPerLastPeriod();

    return statistics;
}

//-------------------------------------------------------------------------------------------------

StatisticsForwarder::StatisticsForwarder(
    std::vector<std::unique_ptr<AbstractCollector>> collectors)
    :
    m_collectors(std::move(collectors))
{
}

void StatisticsForwarder::saveConnectSessionStatistics(const ConnectSession& data)
{
    for (auto& collector: m_collectors)
        collector->saveConnectSessionStatistics(data);
}

//-------------------------------------------------------------------------------------------------

CollectorFactory::CollectorFactory():
    base_type(std::bind(&CollectorFactory::defaultFactoryFunction, this,
        std::placeholders::_1, std::placeholders::_2))
{
}

CollectorFactory& CollectorFactory::instance()
{
    static CollectorFactory collectorInstance;
    return collectorInstance;
}

std::unique_ptr<AbstractCollector> CollectorFactory::defaultFactoryFunction(
    const conf::Statistics& settings,
    nx::sql::AbstractAsyncSqlQueryExecutor* sqlQueryExecutor)
{
    return std::make_unique<PersistentCollector>(settings, sqlQueryExecutor);
}

} // namespace stats
} // namespace hpm
} // namespace nx
