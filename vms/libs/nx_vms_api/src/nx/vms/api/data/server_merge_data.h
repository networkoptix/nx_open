// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api {

/**%apidoc Details about merged server. */
struct NX_VMS_API ServerMergeData
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,

        /**%apidoc Merge success. */
        success,

        /**%apidoc Server is not merged because it was offline when merge is started. */
        offline,

        /**%apidoc Server is not merged because /api/configure returns error. */
        networkError
    )

    /**%apidoc Server Id. */
    QnUuid serverId;

    /**%apidoc Server name. */
    QString serverName;

    /**%apidoc Server merge status. */
    Status status{Status::success};

    /**%apidoc Human readable error message. */
    QString errorMessage;
};
#define ServerMergeData_Fields (serverId)(serverName)(status)(errorMessage)
NX_VMS_API_DECLARE_STRUCT(ServerMergeData)

} // namespace nx::vms::api
