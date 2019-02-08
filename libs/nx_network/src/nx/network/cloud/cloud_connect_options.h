#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

namespace nx {
namespace hpm {
namespace api {

/**
 * Enables or disables cloud connect features, enforced by mediator.
 */
enum CloudConnectOption
{
    emptyCloudConnectOptions = 0,

    /** #CLOUD-824 */
    serverChecksConnectionState = 1,
};

Q_DECLARE_FLAGS(CloudConnectOptions, CloudConnectOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(CloudConnectOptions)

} // namespace api
} // namespace hpm
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::CloudConnectOptions, (lexical), NX_NETWORK_API)
