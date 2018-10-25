#pragma once

#include <memory>

#include <nx_ec/ec_api.h>
#include <utils/common/optional.h>
#include <boost/optional.hpp>

class QnCommonModule;
struct ConfigureSystemData;
struct PasswordData;

namespace nx {
namespace vms {
namespace utils {

struct DbBackupFileData
{
    QString fullPath;
    int build = -1;
    int index = -1;
    qint64 timestamp = -1;
};

/**
* @return Unique filename according to pattern "<prefix>_<build>_<index>.backup" by
* incrementing index
*/
QString makeNextUniqueName(const QString& prefix, int build);

bool backupDatabase(const QString& backupDir,
    std::shared_ptr<ec2::AbstractECConnection> connection);

QList<DbBackupFileData> allBackupFilesData(const QString& backupDir);

void deleteOldBackupFilesIfNeeded(const QString& backupDir, const QString& filePathToSkip,
    qint64 freeSpace);

bool configureLocalPeerAsPartOfASystem(
    QnCommonModule* commonModule,
    const ConfigureSystemData& data);

bool validatePasswordData(const PasswordData& passwordData, QString* errStr);

bool updateUserCredentials(
    std::shared_ptr<ec2::AbstractECConnection> connection,
    PasswordData data,
    QnOptionalBool isEnabled,
    const QnUserResourcePtr& userRes,
    QString* errString = nullptr,
    QnUserResourcePtr* updatedUser = nullptr);

bool resetSystemToStateNew(QnCommonModule* commonModule);

} // namespace utils
} // namespace vms
} // namespace nx
