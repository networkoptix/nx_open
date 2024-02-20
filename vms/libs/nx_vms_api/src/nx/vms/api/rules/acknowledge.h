// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/bookmark_models.h>

#include "event_log.h"

namespace nx::vms::api::rules {

struct NX_VMS_API AcknowledgeBookmark: BookmarkV1
{
    /**%apidoc ID of the event to acknowledge. */
    nx::Uuid eventId;

    AcknowledgeBookmark() = default;
    AcknowledgeBookmark(BookmarkV1&& book): BookmarkV1(std::move(book)) {};
};

#define AcknowledgeBookmark_Fields BookmarkV1_Fields (eventId)
QN_FUSION_DECLARE_FUNCTIONS(AcknowledgeBookmark, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(AcknowledgeBookmark, AcknowledgeBookmark_Fields)

} // namespace nx::vms::rules
