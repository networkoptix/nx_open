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

    if (!m_settings.enabled)
        return;

    m_sqlQueryExecutor->executeUpdate(
        std::bind(&dao::AbstractDataObject::save, m_dataObject.get(), _1, std::move(data)),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult /*dbResult*/)
        {
        });
}

} // namespace stats
} // namespace hpm
} // namespace nx
