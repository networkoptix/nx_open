#include "system_helpers.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

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

struct VmsUtilsFunctionsTag{};
static const auto& kLogTag = typeid(VmsUtilsFunctionsTag);

static QString backupDbFileName(const QString& backupDir, int buildNumber)
{
    return QString("%1_%2_%3.db")
        .arg(closeDirPath(backupDir) + "ecs")
        .arg(buildNumber)
        .arg(qnSyncTime->currentMSecsSinceEpoch());
}

bool backupDatabaseLive(
    const QString& backupDir,
    const ec2::AbstractECConnectionPtr& connection)
{
    const auto buildNumber =
        nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion()).build();

    if (!QDir(backupDir).exists() && !QDir().mkpath(backupDir))
    {
        NX_ERROR(kLogTag, "Failed to create DB backup path %1", backupDir);
        return false;
    }

    const auto fileName = backupDbFileName(backupDir, buildNumber);
    const auto errorCode = connection->dumpDatabaseToFileSync(fileName);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_ERROR(kLogTag, "Failed to dump EC database: %1", ec2::toString(errorCode));
        return false;
    }

    deleteOldBackupFilesIfNeeded(
        backupDir, nx::SystemCommands().freeSpace(backupDir.toStdString()));

    NX_INFO(kLogTag, "Successfully created DB backup %1", fileName);
    return true;
}

bool backupDatabaseViaCopy(const QString& dbFilePath, int buildNumber)
{
    QFileInfo fileInfo(dbFilePath);
    const auto backupDir = fileInfo.absoluteDir();
    const auto backupDirPath = backupDir.canonicalPath();

    if (!backupDir.exists() && !QDir().mkpath(backupDirPath))
    {
        NX_ERROR(kLogTag, "Failed to create DB backup path %1", backupDirPath);
        return false;
    }

    const QString fileName = backupDbFileName(backupDirPath, buildNumber);
    if (!QFile::copy(dbFilePath, fileName))
    {
        NX_ERROR(kLogTag, "Failed to copy DB file %1 to %2", dbFilePath, fileName);
        return false;
    }

    deleteOldBackupFilesIfNeeded(
        backupDirPath, nx::SystemCommands().freeSpace(backupDirPath.toStdString()));

    NX_INFO(kLogTag, "Successfully created DB backup %1", fileName);
    return true;
}

QList<DbBackupFileData> allBackupFilesDataSorted(const QString& backupDir)
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
        if (nameSplits.size() != 3)
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

void deleteOldBackupFilesIfNeeded(const QString& backupDir, qint64 freeSpace)
{
    const qint64 kMaxFreeSpace = 10 * 1024 * 1024LL * 1024LL; //< 10Gb
    const int kMaxBackupFilesCount = freeSpace > kMaxFreeSpace ? 6 : 1;
    const auto allBackupFiles = allBackupFilesDataSorted(backupDir);
    for (int i = kMaxBackupFilesCount; i < allBackupFiles.size(); ++i)
        nx::SystemCommands().removePath(allBackupFiles[i].fullPath.toStdString());
}

} // namespace utils
} // namespace vms
} // namespace nx
