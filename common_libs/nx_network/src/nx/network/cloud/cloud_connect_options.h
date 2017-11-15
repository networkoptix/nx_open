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

// QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::hpm::api::CloudConnectOptions), (lexical))

/**
 * Cannot use QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES macro here since NX_NETWORK_API is needed.
 */

void NX_NETWORK_API serialize(const nx::hpm::api::CloudConnectOptions&, QString*);
bool NX_NETWORK_API deserialize(const QString&, nx::hpm::api::CloudConnectOptions*);
