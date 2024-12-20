// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/json/flags.h>

namespace nx::hpm::api {

/**
 * Enables or disables cloud connect features, enforced by mediator.
 */
NX_REFLECTION_ENUM(CloudConnectOption,
    /** #CLOUD-824 */
    serverChecksConnectionState = 1
)

Q_DECLARE_FLAGS(CloudConnectOptions, CloudConnectOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(CloudConnectOptions)

} // namespace nx::hpm::api
