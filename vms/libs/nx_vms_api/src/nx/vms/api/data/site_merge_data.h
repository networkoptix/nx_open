// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API SiteMergeData
{
    /**%apidoc The SocketAddress of remote Site server. */
    std::string remoteEndpoint; //< TODO: Is SocketAddress to be used?

    /**%apidoc The token should be fresh obtained from `/rest/v{1-}/login/sessions`. */
    std::string remoteSessionToken;

    /**%apidoc[opt] */
    std::string remoteCertificatePem;

    /**%apidoc[opt] */
    QnUuid remoteServerId;

    /**%apidoc[opt] Whether remote Site settings should be taken. */
    bool takeRemoteSettings = false;

    /**%apidoc[opt] Merge only one Server from remote Site. */
    bool mergeOneServer = false;

    /**%apidoc[opt] Whether incompatible Site should be ignored. */
    bool ignoreIncompatible = false;

    /**%apidoc[opt] Whether found Server duplicates should be ignored if they are offline. */
    bool ignoreOfflineServerDuplicates = false;

    /**%apidoc[opt] No actual merge if true. */
    bool dryRun = false;
};
#define SiteMergeData_Fields \
    (remoteEndpoint) \
    (remoteSessionToken) \
    (remoteCertificatePem) \
    (remoteServerId) \
    (takeRemoteSettings) \
    (mergeOneServer) \
    (ignoreIncompatible) \
    (ignoreOfflineServerDuplicates) \
    (dryRun)
QN_FUSION_DECLARE_FUNCTIONS(SiteMergeData, (json), NX_VMS_API)

} // namespace nx::vms::api
