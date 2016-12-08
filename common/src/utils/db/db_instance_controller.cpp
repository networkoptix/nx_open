#include "db_instance_controller.h"

#include <thread>

#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace db {

InstanceController::InstanceController(const ConnectionOptions& dbConnectionOptions):
    m_dbConnectionOptions(dbConnectionOptions),
    m_queryExecutor(std::make_unique<AsyncSqlQueryExecutor>(dbConnectionOptions)),
    m_dbStructureUpdater(m_queryExecutor.get())
{
}

bool InstanceController::initialize()
{
    if (!m_queryExecutor->init())
    {
        NX_LOG(lit("Failed to open connection to DB"), cl_logALWAYS);
        return false;
    }

    if (!updateDbStructure())
    {
        NX_LOG("Could not update DB to current vesion", cl_logALWAYS);
        return false;
    }

    if (!configureDb())
    {
        NX_LOG("Failed to tune DB", cl_logALWAYS);
        return false;
    }

    return true;
}

const std::unique_ptr<AsyncSqlQueryExecutor>& InstanceController::queryExecutor()
{
    return m_queryExecutor;
}

DBStructureUpdater& InstanceController::dbStructureUpdater()
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

    std::promise<DBResult> cacheFilledPromise;
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
} // namespace nx
