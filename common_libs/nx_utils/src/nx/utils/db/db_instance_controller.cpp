#include "db_instance_controller.h"

#include <thread>
#include <string>

#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {
namespace db {

constexpr static std::chrono::minutes kDefaultStatisticsAggregationPeriod = std::chrono::minutes(1);
static const std::string kCdbStructureName = "cdb_BF58C070-B0E6-4327-BB2E-417A68AAA53D";

InstanceController::InstanceController(const ConnectionOptions& dbConnectionOptions):
    m_dbConnectionOptions(dbConnectionOptions),
    m_statisticsCollector(kDefaultStatisticsAggregationPeriod),
    m_queryExecutor(std::make_unique<AsyncSqlQueryExecutor>(dbConnectionOptions)),
    m_dbStructureUpdater(kCdbStructureName, m_queryExecutor.get())
{
    m_queryExecutor->setStatisticsCollector(&m_statisticsCollector);
}

bool InstanceController::initialize()
{
    NX_DEBUG(this, lm("Initializing query executor"));
    if (!m_queryExecutor->init())
    {
        NX_ERROR(this, "Failed to open connection to DB");
        return false;
    }

    NX_DEBUG(this, lm("Updating DB structure"));
    if (!updateDbStructure())
    {
        NX_ERROR(this, "Could not update DB to current vesion");
        return false;
    }

    NX_DEBUG(this, lm("Configuring DB"));
    if (!configureDb())
    {
        NX_ERROR(this, "Failed to tune DB");
        return false;
    }

    return true;
}

AsyncSqlQueryExecutor& InstanceController::queryExecutor()
{
    return *m_queryExecutor;
}

const AsyncSqlQueryExecutor& InstanceController::queryExecutor() const
{
    return *m_queryExecutor;
}

const StatisticsCollector& InstanceController::statisticsCollector() const
{
    return m_statisticsCollector;
}

StatisticsCollector& InstanceController::statisticsCollector()
{
    return m_statisticsCollector;
}

DbStructureUpdater& InstanceController::dbStructureUpdater()
{
    return m_dbStructureUpdater;
}

bool InstanceController::updateDbStructure()
{
    return m_dbStructureUpdater.updateStructSync();
}

bool InstanceController::configureDb()
{
    using namespace std::placeholders;

    if (m_dbConnectionOptions.driverType != RdbmsDriverType::sqlite)
        return true;

    nx::utils::promise<DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    m_queryExecutor->executeUpdateWithoutTran(
        std::bind(&InstanceController::configureSqliteInstance, this, _1),
        [&](QueryContext* /*queryContext*/, DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    return future.get() == DBResult::ok;
}

DBResult InstanceController::configureSqliteInstance(
    QueryContext* queryContext)
{
    QSqlQuery enableWalQuery(*queryContext->connection());
    enableWalQuery.prepare(lit("PRAGMA journal_mode = WAL"));
    if (!enableWalQuery.exec())
    {
        NX_LOG(lit("sqlite configure. Failed to enable WAL mode. %1")
            .arg(enableWalQuery.lastError().text()),
            cl_logWARNING);
        return DBResult::ioError;
    }

    QSqlQuery enableFKQuery(*queryContext->connection());
    enableFKQuery.prepare(lit("PRAGMA foreign_keys = ON"));
    if (!enableFKQuery.exec())
    {
        NX_LOG(lit("sqlite configure. Failed to enable foreign keys. %1")
            .arg(enableFKQuery.lastError().text()),
            cl_logWARNING);
        return DBResult::ioError;
    }

    //QSqlQuery setLockingModeQuery(*connection);
    //setLockingModeQuery.prepare("PRAGMA locking_mode = NORMAL");
    //if (!setLockingModeQuery.exec())
    //{
    //    NX_LOG(lit("sqlite configure. Failed to set locking mode. %1")
    //        .arg(setLockingModeQuery.lastError().text()), cl_logWARNING);
    //    return DBResult::ioError;
    //}

    return DBResult::ok;
}

} // namespace db
} // namespace utils
} // namespace nx
