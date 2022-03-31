// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API SystemMergeData
{
    /**%apidoc The SocketAddress of remote system server. */
    std::string remoteEndpoint; //< TODO: Is SocketAddress to be used?

    /**%apidoc The token should be fresh obtained from `/rest/v{1-}/login/sessions`. */
    std::string remoteSessionToken;

    /**%apidoc[opt] */
    std::string remoteCertificatePem;

    /**%apidoc[opt] */
    QnUuid remoteServerId;

    /**%apidoc[opt] Whether remote System settings should be taken. */
    bool takeRemoteSettings = false;

    /**%apidoc[opt] Merge only one Server from remote System. */
    bool mergeOneServer = false;

    /**%apidoc[opt] Whether incompatible System should be ignored. */
    bool ignoreIncompatible = false;

    /**%apidoc[opt] Whether found Server duplicates should be ignored if they are offline. */
    bool ignoreOfflineServerDuplicates = false;

    /**%apidoc[opt] No actual merge if true. */
    bool dryRun = false;
};
#define SystemMergeData_Fields \
    (remoteEndpoint) \
    (remoteSessionToken) \
    (remoteCertificatePem) \
    (remoteServerId) \
    (takeRemoteSettings) \
    (mergeOneServer) \
    (ignoreIncompatible) \
    (ignoreOfflineServerDuplicates) \
    (dryRun)
QN_FUSION_DECLARE_FUNCTIONS(SystemMergeData, (json), NX_VMS_API)

} // namespace nx::vms::api
