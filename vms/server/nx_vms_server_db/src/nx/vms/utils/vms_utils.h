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
