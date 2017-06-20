#include "db_instance_controller.h"

#include <nx/utils/log/log.h>

#include "structure_update_statements.h"
#include "../../ec2/db/migration/add_history_to_transaction.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

namespace {

constexpr auto kDbRepeatedConnectionAttemptDelay = std::chrono::seconds(5);

} // namespace

DbInstanceController::DbInstanceController(
    const nx::db::ConnectionOptions& dbConnectionOptions,
    boost::optional<unsigned int> dbVersionToUpdateTo)
    :
    nx::db::InstanceController(dbConnectionOptions),
    m_userAuthRecordsMigrationNeeded(false)
{
    initializeStructureMigration();
    if (dbVersionToUpdateTo)
        dbStructureUpdater().setVersionToUpdateTo(*dbVersionToUpdateTo);

    if (!initialize())
    {
        NX_LOG(lit("Failed to initialize DB connection"), cl_logALWAYS);
        throw nx::db::Exception(
            nx::db::DBResult::ioError,
            "Failed to initialize connection to DB");
    }
}

bool DbInstanceController::isUserAuthRecordsMigrationNeeded() const
{
    return m_userAuthRecordsMigrationNeeded;
}

void DbInstanceController::initializeStructureMigration()
{
    dbStructureUpdater().addFullSchemaScript(13, db::kCreateDbVersion13);

    dbStructureUpdater().addUpdateScript(db::kCreateAccountData);
    dbStructureUpdater().addUpdateScript(db::kCreateSystemData);
    dbStructureUpdater().addUpdateScript(db::kSystemToAccountMapping);
    dbStructureUpdater().addUpdateScript(db::kAddCustomizationToSystem);
    dbStructureUpdater().addUpdateScript(db::kAddCustomizationToAccount);
    dbStructureUpdater().addUpdateScript(db::kAddTemporaryAccountPassword);
    dbStructureUpdater().addUpdateScript(db::kAddIsEmailCodeToTemporaryAccountPassword);
    dbStructureUpdater().addUpdateScript(db::kRenameSystemAccessRoles);
    dbStructureUpdater().addUpdateScript(db::kChangeSystemIdTypeToString);
    dbStructureUpdater().addUpdateScript(db::kAddDeletedSystemState);
    dbStructureUpdater().addUpdateScript(db::kSystemExpirationTime);
    dbStructureUpdater().addUpdateScript(db::kReplaceBlobWithVarchar);
    dbStructureUpdater().addUpdateScript(db::kTemporaryAccountCredentials);
    dbStructureUpdater().addUpdateScript(db::kTemporaryAccountCredentialsProlongationPeriod);
    dbStructureUpdater().addUpdateScript(db::kAddCustomAndDisabledAccessRoles);
    dbStructureUpdater().addUpdateScript(db::kAddMoreFieldsToSystemSharing);
    dbStructureUpdater().addUpdateScript(db::kAddVmsUserIdToSystemSharing);
    dbStructureUpdater().addUpdateScript(db::kAddSystemTransactionLog);
    dbStructureUpdater().addUpdateScript(db::kChangeTransactionLogTimestampTypeToBigInt);
    dbStructureUpdater().addUpdateScript(db::kAddPeerSequence);
    dbStructureUpdater().addUpdateScript(db::kAddSystemSequence);
    dbStructureUpdater().addUpdateScript(db::kMakeTransactionTimestamp128Bit);
    dbStructureUpdater().addUpdateScript(db::kAddSystemUsageFrequency);
    dbStructureUpdater().addUpdateFunc(&ec2::migration::addHistoryToTransaction::migrate);
    dbStructureUpdater().addUpdateScript(db::kAddInviteHasBeenSentAccountStatus);
    dbStructureUpdater().addUpdateScript(db::kAddHa1CalculatedUsingSha256);
    dbStructureUpdater().addUpdateScript(db::kAddVmsOpaqueData);
    dbStructureUpdater().addUpdateScript(db::kDropGlobalTransactionSequenceTable);
    dbStructureUpdater().addUpdateScript(db::kRenameGroupToRole);
    dbStructureUpdater().addUpdateScript(db::kSetIsEnabledToTrueWhereUndefined);
    dbStructureUpdater().addUpdateScript(
        {{nx::db::RdbmsDriverType::mysql, db::kRestoreSystemToAccountReferenceUniquenessMySql},
         {nx::db::RdbmsDriverType::unknown, db::kRestoreSystemToAccountReferenceUniquenessSqlite}});
    dbStructureUpdater().addUpdateScript(db::kAddAccountTimestamps);
    dbStructureUpdater().addUpdateScript(db::kAddSystemRegistrationTimestamp);
    dbStructureUpdater().addUpdateScript(db::kAddSystemHealthStateHistory);

    // Version 3.1.

    dbStructureUpdater().addUpdateScript(db::kAddSystemUserAuthInfo);
    dbStructureUpdater().addUpdateScript(db::kAddSystemNonce);
    dbStructureUpdater().addUpdateFunc(
        [this](nx::db::QueryContext*)
        {
            m_userAuthRecordsMigrationNeeded = true;
            return nx::db::DBResult::ok;
        });
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
