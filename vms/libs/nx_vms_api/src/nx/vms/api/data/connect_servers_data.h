// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API ConnnectServersData
{
    nx::Uuid getId() const { return id; }

    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    nx::Uuid id;

    /**%apidoc Remote Server id. */
    nx::Uuid remoteServerId;

    /**%apidoc[opt] The certificate of the remote Server. It will be checked if provided. */
    std::string remoteCertificatePem;
};
#define ConnnectServersData_Fields (id)(remoteServerId)(remoteCertificatePem)
QN_FUSION_DECLARE_FUNCTIONS(ConnnectServersData, (json), NX_VMS_API)

} // namespace nx::vms::api
