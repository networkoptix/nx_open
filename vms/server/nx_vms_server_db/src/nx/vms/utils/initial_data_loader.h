#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api.h>

class QnCommonModule;
class QnCommonMessageProcessor;

namespace nx {
namespace vms {
namespace utils {

/**
 * @param mediaServer If not null, then status of this server is reset to Online. TODO Refactor out.
 * @param needToStop Periodically checked. If returns true, then execution is halted and function returns.
 */
void loadResourcesFromEcs(
    QnCommonModule* commonModule,
    ec2::AbstractECConnectionPtr ec2Connection,
    QnCommonMessageProcessor* messageProcessor,
    QnMediaServerResourcePtr mediaServer,
    std::function<bool()> needToStop);

} // namespace utils
} // namespace vms
} // namespace nx
