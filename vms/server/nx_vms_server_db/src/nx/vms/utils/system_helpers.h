#pragma once

#include <memory>

#include <nx_ec/ec_api.h>
#include <utils/common/optional.h>
#include <boost/optional.hpp>

namespace nx {
namespace vms {
namespace utils {

struct DbBackupFileData
{
    QString fullPath;
    int build = -1;
    qint64 timestamp = -1;
};

/**
 * Should be used when server is 'alive', i.e. Server Module has been initialized,
 * transaction engine is operational.
 * Note: Build number to make backup file name is taken from AppInfo. It means it corresponds
 *     currently running server version.
 */
bool backupDatabaseLive(
    const QString& backupDir,
    const ec2::AbstractECConnectionPtr& connection);

/**
 * Should be used when database file has not been opened yet.
 */
bool backupDatabaseViaCopy(const QString& dbFilePath, int buildNumber);

QList<DbBackupFileData> allBackupFilesDataSorted(const QString& backupDir);

/* Newest files come first */
void deleteOldBackupFilesIfNeeded(const QString& backupDir, qint64 freeSpace);

} // namespace utils
} // namespace vms
} // namespace nx
