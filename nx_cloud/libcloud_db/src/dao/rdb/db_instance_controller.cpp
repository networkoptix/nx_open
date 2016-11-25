#include "db_instance_controller.h"

#include <thread>

#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "structure_update_statements.h"
#include "ec2/db/migration/add_history_to_transaction.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

namespace {

constexpr auto kDbRepeatedConnectionAttemptDelay = std::chrono::seconds(5);

} // namespace

DbInstanceController::DbInstanceController(const nx::db::ConnectionOptions& dbConnectionOptions):
    m_dbConnectionOptions(dbConnectionOptions),
    m_queryExecutor(std::make_unique<nx::db::AsyncSqlQueryExecutor>(dbConnectionOptions)),
    m_dbStructureUpdater(m_queryExecutor.get())
{
    m_dbStructureUpdater.addFullSchemaScript(13, db::kCreateDbVersion13);

    m_dbStructureUpdater.addUpdateScript(db::kCreateAccountData);
    m_dbStructureUpdater.addUpdateScript(db::kCreateSystemData);
    m_dbStructureUpdater.addUpdateScript(db::kSystemToAccountMapping);
    m_dbStructureUpdater.addUpdateScript(db::kAddCustomizationToSystem);
    m_dbStructureUpdater.addUpdateScript(db::kAddCustomizationToAccount);
    m_dbStructureUpdater.addUpdateScript(db::kAddTemporaryAccountPassword);
    m_dbStructureUpdater.addUpdateScript(db::kAddIsEmailCodeToTemporaryAccountPassword);
    m_dbStructureUpdater.addUpdateScript(db::kRenameSystemAccessRoles);
    m_dbStructureUpdater.addUpdateScript(db::kChangeSystemIdTypeToString);
    m_dbStructureUpdater.addUpdateScript(db::kAddDeletedSystemState);
    m_dbStructureUpdater.addUpdateScript(db::kSystemExpirationTime);
    m_dbStructureUpdater.addUpdateScript(db::kReplaceBlobWithVarchar);
    m_dbStructureUpdater.addUpdateScript(db::kTemporaryAccountCredentials);
    m_dbStructureUpdater.addUpdateScript(db::kTemporaryAccountCredentialsProlongationPeriod);
    m_dbStructureUpdater.addUpdateScript(db::kAddCustomAndDisabledAccessRoles);
    m_dbStructureUpdater.addUpdateScript(db::kAddMoreFieldsToSystemSharing);
    m_dbStructureUpdater.addUpdateScript(db::kAddVmsUserIdToSystemSharing);
    m_dbStructureUpdater.addUpdateScript(db::kAddSystemTransactionLog);
    m_dbStructureUpdater.addUpdateScript(db::kChangeTransactionLogTimestampTypeToBigInt);
    m_dbStructureUpdater.addUpdateScript(db::kAddPeerSequence);
    m_dbStructureUpdater.addUpdateScript(db::kAddSystemSequence);
    m_dbStructureUpdater.addUpdateScript(db::kMakeTransactionTimestamp128Bit);
    m_dbStructureUpdater.addUpdateScript(db::kAddSystemUsageFrequency);
    m_dbStructureUpdater.addUpdateFunc(&ec2::migration::addHistoryToTransaction::migrate);
    m_dbStructureUpdater.addUpdateScript(db::kAddInviteHasBeenSentAccountStatus);
    m_dbStructureUpdater.addUpdateScript(db::kAddHa1CalculatedUsingSha256);
    m_dbStructureUpdater.addUpdateScript(db::kAddVmsOpaqueData);
    m_dbStructureUpdater.addUpdateScript(db::kDropGlobalTransactionSequenceTable);
    m_dbStructureUpdater.addUpdateScript(db::kRenameGroupToRole);
}

bool DbInstanceController::initialize()
{
    for (;;)
    {
        if (m_queryExecutor->init())
            break;
        NX_LOG(lit("Cannot start application due to DB connect error"), cl_logALWAYS);
        std::this_thread::sleep_for(kDbRepeatedConnectionAttemptDelay);
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

const std::unique_ptr<nx::db::AsyncSqlQueryExecutor>& DbInstanceController::queryExecutor()
{
    return m_queryExecutor;
}

bool DbInstanceController::updateDbStructure()
{
    return m_dbStructureUpdater.updateStructSync();
}

bool DbInstanceController::configureDb()
{
    using namespace std::placeholders;

    if (m_dbConnectionOptions.driverType != nx::db::RdbmsDriverType::sqlite)
        return true;

    std::promise<nx::db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    m_queryExecutor->executeUpdateWithoutTran(
        std::bind(&DbInstanceController::configureSqliteInstance, this, _1),
        [&](nx::db::QueryContext* /*queryContext*/, nx::db::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    return future.get() == nx::db::DBResult::ok;
}

nx::db::DBResult DbInstanceController::configureSqliteInstance(
    nx::db::QueryContext* queryContext)
{
    QSqlQuery enableWalQuery(*queryContext->connection());
    enableWalQuery.prepare("PRAGMA journal_mode = WAL");
    if (!enableWalQuery.exec())
    {
        NX_LOG(lit("sqlite configure. Failed to enable WAL mode. %1")
            .arg(enableWalQuery.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    QSqlQuery enableFKQuery(*queryContext->connection());
    enableFKQuery.prepare("PRAGMA foreign_keys = ON");
    if (!enableFKQuery.exec())
    {
        NX_LOG(lit("sqlite configure. Failed to enable foreign keys. %1")
            .arg(enableFKQuery.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    //QSqlQuery setLockingModeQuery(*connection);
    //setLockingModeQuery.prepare("PRAGMA locking_mode = NORMAL");
    //if (!setLockingModeQuery.exec())
    //{
    //    NX_LOG(lit("sqlite configure. Failed to set locking mode. %1")
    //        .arg(setLockingModeQuery.lastError().text()), cl_logWARNING);
    //    return nx::db::DBResult::ioError;
    //}

    return nx::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
