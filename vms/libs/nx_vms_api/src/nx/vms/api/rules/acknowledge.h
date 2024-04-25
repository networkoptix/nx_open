// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/bookmark_models.h>

#include "event_log.h"

namespace nx::vms::api::rules {

struct NX_VMS_API AcknowledgeBookmark: BookmarkBase
{
    /**%apidoc ID of the notification action to acknowledge. */
    nx::Uuid actionId;

    /**%apidoc ID of the server holding event log record to acknowledge. */
    nx::Uuid actionServerId;

    void setIds(nx::Uuid /*bookmarkId*/, nx::Uuid serverId) { actionServerId = serverId; };
    nx::Uuid bookmarkId() const { return nx::Uuid::createUuid(); };
};

#define AcknowledgeBookmark_Fields BookmarkBase_Fields (actionId)(actionServerId)
QN_FUSION_DECLARE_FUNCTIONS(AcknowledgeBookmark, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(AcknowledgeBookmark, AcknowledgeBookmark_Fields)

} // namespace nx::vms::rules
