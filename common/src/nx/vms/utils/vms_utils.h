#pragma once

#include <memory>

#include <nx_ec/ec_api.h>
#include <utils/common/optional.h>

class QnCommonModule;
struct ConfigureSystemData;
struct PasswordData;

namespace nx {
namespace vms {
namespace utils {

/**
* @return Unique filename according to pattern "<prefix>_<build>_<index>.backup" by
* incrementing index
*/
    QString makeNextUniqueName(const QString& prefix, int build);

bool backupDatabase(
    const QString& outputDir,
    std::shared_ptr<ec2::AbstractECConnection> connection);

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
