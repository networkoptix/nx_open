#include "collector.h"

#include "dao/data_object_factory.h"

namespace nx {
namespace hpm {
namespace stats {

Collector::Collector(
    const conf::Statistics& settings,
    nx::utils::db::AsyncSqlQueryExecutor* sqlQueryExecutor)
    :
    m_settings(settings),
    m_sqlQueryExecutor(sqlQueryExecutor),
    m_dataObject(dao::DataObjectFactory::create())
{
}

Collector::~Collector()
{
    m_startedAsyncCallsCounter.wait();
}

void Collector::saveConnectSessionStatistics(ConnectSession data)
{
    using namespace std::placeholders;

    m_sqlQueryExecutor->executeUpdate(
        std::bind(&dao::AbstractDataObject::save, m_dataObject.get(), _1, std::move(data)),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult /*dbResult*/)
        {
        });
}

//-------------------------------------------------------------------------------------------------

void DummyCollector::saveConnectSessionStatistics(ConnectSession /*data*/)
{
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
    nx::utils::db::AsyncSqlQueryExecutor* sqlQueryExecutor)
{
    return std::make_unique<Collector>(settings, sqlQueryExecutor);
}

} // namespace stats
} // namespace hpm
} // namespace nx
