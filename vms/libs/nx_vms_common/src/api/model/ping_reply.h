// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/timestamp.h>

/** Server response to `ping` HTTP API request. */
struct QnPingReply
{
    QnPingReply(): sysIdTime(0) {}

    /**%apidoc Id of the Server Module. */
    QnUuid moduleGuid;

    /**%apidoc Id of the Site. */
    QnUuid localSystemId;

    /**%apidoc[proprietary]*/
    qint64 sysIdTime;

    /**%apidoc[proprietary]*/
    nx::vms::api::Timestamp tranLogTime;
};
#define QnPingReply_Fields (moduleGuid)(localSystemId)(sysIdTime)(tranLogTime)

NX_REFLECTION_INSTRUMENT(QnPingReply, QnPingReply_Fields);
QN_FUSION_DECLARE_FUNCTIONS(QnPingReply, (json), NX_VMS_COMMON_API)
