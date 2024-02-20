// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_instance_controller.h"

#include <thread>
#include <string>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "test_support/query_executor_factory.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx::sql {

static const std::string kCdbStructureName = "cdb_BF58C070-B0E6-4327-BB2E-417A68AAA53D";

InstanceController::InstanceController(const ConnectionOptions& dbConnectionOptions):
    m_dbConnectionOptions(dbConnectionOptions),
    m_queryExecutor(QueryExecutorProviderFactory::instance().create(dbConnectionOptions)),
    m_dbStructureUpdater(kCdbStructureName, m_queryExecutor.get())
{
}

bool InstanceController::initialize()
{
    NX_DEBUG(this, "Initializing DB %1", m_dbConnectionOptions.dbName);
    if (!m_queryExecutor->init())
    {
        NX_ERROR(this, "Failed to open connection to DB %1", m_dbConnectionOptions.dbName);
        return false;
    }

    NX_DEBUG(this, "Configuring DB %1", m_dbConnectionOptions.dbName);
    if (!configureDb())
    {
        NX_ERROR(this, "Failed to tune DB %1", m_dbConnectionOptions.dbName);
        if (m_dbConnectionOptions.failOnDbTuneError)
            return false;
    }

    NX_DEBUG(this, "Updating DB structure %1", m_dbConnectionOptions.dbName);
    if (!updateDbStructure())
    {
        NX_ERROR(this, "Could not update DB %1 to current version", m_dbConnectionOptions.dbName);
        return false;
    }

    NX_DEBUG(this, nx::format("DB initialized"));

    return true;
}

void InstanceController::stop()
{
    m_queryExecutor->pleaseStopSync();
}

AbstractAsyncSqlQueryExecutor& InstanceController::queryExecutor()
{
    return *m_queryExecutor;
}

const AbstractAsyncSqlQueryExecutor& InstanceController::queryExecutor() const
{
    return *m_queryExecutor;
}

const StatisticsCollector& InstanceController::statisticsCollector() const
{
    return m_queryExecutor->statisticsCollector();
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
        &InstanceController::configureSqliteInstance,
        [&](DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    return future.get() == DBResult::ok;
}

DBResult InstanceController::configureSqliteInstance(QueryContext* queryContext)
{
    QSqlQuery enableAutoVacuumQuery(*queryContext->connection()->qtSqlConnection());
    enableAutoVacuumQuery.prepare("PRAGMA auto_vacuum = 1");
    if (!enableAutoVacuumQuery.exec())
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            nx::format("Failed to enable auto vacuum mode. %1")
                .arg(enableAutoVacuumQuery.lastError().text()));
        return DBResult::ioError;
    }

    QSqlQuery enableWalQuery(*queryContext->connection()->qtSqlConnection());
    enableWalQuery.prepare("PRAGMA journal_mode = WAL");
    if (!enableWalQuery.exec())
    {
        NX_WARNING(NX_SCOPE_TAG, nx::format("Failed to enable WAL mode. %1")
            .arg(enableWalQuery.lastError().text()));
        return DBResult::ioError;
    }

    QSqlQuery enableFKQuery(*queryContext->connection()->qtSqlConnection());
    enableFKQuery.prepare("PRAGMA foreign_keys = ON");
    if (!enableFKQuery.exec())
    {
        NX_WARNING(NX_SCOPE_TAG, nx::format("Failed to enable foreign keys. %1")
            .arg(enableFKQuery.lastError().text()));
        return DBResult::ioError;
    }

    //QSqlQuery setLockingModeQuery(*connection);
    //setLockingModeQuery.prepare("PRAGMA locking_mode = NORMAL");
    //if (!setLockingModeQuery.exec())
    //{
    //    NX_WARNING(this, nx::format("sqlite configure. Failed to set locking mode. %1")
    //        .arg(setLockingModeQuery.lastError().text()));
    //    return DBResult::ioError;
    //}

    return DBResult::ok;
}

} // namespace nx::sql
