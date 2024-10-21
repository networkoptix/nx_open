// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>

#include <nx/reflect/instrument.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(ServerModelFlag,
    none = 0,

    allowsAdminApiForPowerUsers = 1 << 0,

    hasInternetConnection = 1 << 1,

    /**%apidoc The Server has a setting isArmServer=true. */
    isArmServer = 1 << 2,

    isEdge = 1 << 3,

    /**%apidoc Server runs inside a Docker container. */
    runsInDocker = 1 << 4,

    /**%apidoc Server has a buzzer that can produce sound. */
    supportsBuzzer = 1 << 5,

    /**%apidoc Server can provide information about internal fan and its state. */
    supportsFanMonitoring = 1 << 6,

    /**%apidoc Server can provide information about built-in PoE block. */
    supportsPoeManagement = 1 << 7,

    supportsTranscoding = 1 << 8
);
Q_DECLARE_FLAGS(ServerModelFlags, ServerModelFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ServerModelFlags)

NX_VMS_API ServerModelFlags convertServerFlagsToModel(ServerFlags flags);
NX_VMS_API ServerFlags convertModelToServerFlags(ServerModelFlags flags);

} // namespace nx::vms::api
