#include "system_helpers.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <api/model/configure_system_data.h>
#include <api/resource_property_adaptor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <network/system_helpers.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/utils/app_info.h>
#include <nx/utils/password_analyzer.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/system_commands.h>

namespace nx {
namespace vms {
namespace utils {

bool backupDatabase(const QString& backupDir,
    std::shared_ptr<ec2::AbstractECConnection> connection,
    const QString& reason,
    const boost::optional<QString>& dbFilePath,
    const boost::optional<int>& buildNumber)
{
    const auto buildNumberArg = buildNumber
        ? *buildNumber
        : nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion()).build();

    const QString fileName = lm("%1_%2_%3_%4.db").args(closeDirPath(backupDir) + "ecs",
        buildNumberArg, qnSyncTime->currentMSecsSinceEpoch(), reason);

    QDir dir(backupDir);
    if (!dir.exists() && !dir.mkdir(backupDir))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to create DB backup directory %1", dir);
        return false;
    }

    if (!dbFilePath)
    {
        const ec2::ErrorCode errorCode = connection->dumpDatabaseToFileSync(fileName);
        if (errorCode != ec2::ErrorCode::ok)
        {
            NX_ERROR(NX_SCOPE_TAG, "Failed to dump EC database %1: %2",
                fileName, ec2::toString(errorCode));
            return false;
        }
    }
    else
    {
        if (!QFile::copy(*dbFilePath, fileName))
        {
            NX_WARNING(NX_SCOPE_TAG, "Failed to copy DB %1 to %2", *dbFilePath, fileName);
            return false;
        }
    }

    const auto freeSpace = nx::SystemCommands().freeSpace(backupDir.toStdString());
    deleteOldBackupFilesIfNeeded(backupDir, freeSpace, reason);

    NX_WARNING(NX_SCOPE_TAG, "Successfully created DB backup %1", fileName);
    return true;
}

QList<DbBackupFileData> allBackupFilesDataSorted(const QString& backupDir, const QString& reason)
{
    QDir dir(backupDir);
    QList<DbBackupFileData> result;

    if (!dir.exists())
        return result;

    for (const auto& fileInfo: dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files))
    {
        if (fileInfo.completeSuffix() != "db")
            continue;

        auto nameSplits = fileInfo.baseName().split('_');
        if (nameSplits.size() != 3 && nameSplits.size() != 4)
            continue;

        if (!reason.isEmpty() && nameSplits.size() == 4 && nameSplits[3] != reason)
            continue;

        DbBackupFileData backupFileData;
        backupFileData.fullPath = fileInfo.absoluteFilePath();

        bool ok;
        bool conversionSuccessfull = true;
        backupFileData.build = nameSplits[1].toInt(&ok);
        conversionSuccessfull &= ok;
        backupFileData.timestamp = nameSplits[2].toLongLong(&ok);
        conversionSuccessfull &= ok;

        if (!conversionSuccessfull)
            continue;

        result.push_back(backupFileData);
    }

    std::sort(result.begin(), result.end(),
        [](const nx::vms::utils::DbBackupFileData& lhs,
            const nx::vms::utils::DbBackupFileData& rhs)
        {
            return lhs.timestamp > rhs.timestamp;
        });


    return result;
}

void deleteOldBackupFilesIfNeeded(const QString& backupDir, qint64 freeSpace, const QString& reason)
{
    const qint64 kMaxFreeSpace = 10 * 1024 * 1024LL * 1024LL; //< 10Gb
    const int kMaxBackupFilesCount = freeSpace > kMaxFreeSpace ? 6 : 1;
    const auto allBackupFiles = allBackupFilesDataSorted(backupDir, reason);
    for (int i = kMaxBackupFilesCount; i < allBackupFiles.size(); ++i)
        nx::SystemCommands().removePath(allBackupFiles[i].fullPath.toStdString());
}

} // namespace utils
} // namespace vms
} // namespace nx
