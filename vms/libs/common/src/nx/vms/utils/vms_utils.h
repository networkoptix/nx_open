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
    qint64 timestamp = -1;
};

bool backupDatabase(const QString& backupDir,
    std::shared_ptr<ec2::AbstractECConnection> connection,
    const boost::optional<QString>& dbFilePath = boost::none,
    const boost::optional<int>& buildNumber = boost::none);

QList<DbBackupFileData> allBackupFilesDataSorted(const QString& backupDir);

/* Newest files come first */
void deleteOldBackupFilesIfNeeded(const QString& backupDir, qint64 freeSpace);

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
