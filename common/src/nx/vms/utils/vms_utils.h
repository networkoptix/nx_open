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

} // namespace utils
} // namespace vms
} // namespace nx
