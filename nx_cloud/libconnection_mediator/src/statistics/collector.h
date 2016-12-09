#pragma once

#include <memory>

#include <utils/common/counter.h>
#include <utils/db/async_sql_query_executor.h>

#include "connection_statistics_info.h"
#include "dao/abstract_data_object.h"
#include "settings.h"

namespace nx {
namespace hpm {
namespace stats {

class Collector
{
public:
    Collector(
        const conf::Statistics& settings,
        nx::db::AsyncSqlQueryExecutor* sqlQueryExecutor);
    ~Collector();

    void saveConnectSessionStatistics(ConnectSession data);

private:
    const conf::Statistics m_settings;
    nx::db::AsyncSqlQueryExecutor* m_sqlQueryExecutor;
    std::unique_ptr<dao::AbstractDataObject> m_dataObject;
    QnCounter m_startedAsyncCallsCounter;
};

} // namespace stats
} // namespace hpm
} // namespace nx
