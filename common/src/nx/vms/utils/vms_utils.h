#pragma once

#include <memory>

#include <nx_ec/ec_api.h>

class QnCommonModule;
struct ConfigureSystemData;

namespace nx {
namespace vms {
namespace utils {

bool backupDatabase(
    const QString& outputDir,
    std::shared_ptr<ec2::AbstractECConnection> connection);

bool configureLocalPeerAsPartOfASystem(
    QnCommonModule* commonModule,
    const ConfigureSystemData& data);

} // namespace utils
} // namespace vms
} // namespace nx
