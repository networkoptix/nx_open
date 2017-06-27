#pragma once

#include <memory>

#include <nx/utils/counter.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include "connection_statistics_info.h"
#include "dao/abstract_data_object.h"
#include "settings.h"

namespace nx {
namespace hpm {
namespace stats {

class AbstractCollector
{
public:
    virtual ~AbstractCollector() = default;

    virtual void saveConnectSessionStatistics(ConnectSession data) = 0;
};

class Collector:
    public AbstractCollector
{
public:
    Collector(
        const conf::Statistics& settings,
        nx::utils::db::AsyncSqlQueryExecutor* sqlQueryExecutor);
    virtual ~Collector() override;

    virtual void saveConnectSessionStatistics(ConnectSession data) override;

private:
    const conf::Statistics m_settings;
    nx::utils::db::AsyncSqlQueryExecutor* m_sqlQueryExecutor;
    std::unique_ptr<dao::AbstractDataObject> m_dataObject;
    nx::utils::Counter m_startedAsyncCallsCounter;
};

} // namespace stats
} // namespace hpm
} // namespace nx
