// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_facade.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/bookmark_models.h>

namespace nx::vms::common {

QString BookmarkFacadeStrings::creatorName(
    const std::optional<nx::Uuid>& creatorUserId, QnResourcePool* pool)
{
    if (!creatorUserId || creatorUserId->isNull())
        return {};

    if (*creatorUserId == QnCameraBookmark::systemUserId())
        return siteEvent();

    const auto userResource =
        pool->getResourceById<QnUserResource>(*creatorUserId);

    return userResource ? userResource->getName() : QString{};
}

QString BookmarkFacadeStrings::cameraName(const nx::Uuid& deviceId, QnResourcePool* pool)
{
    if (!NX_ASSERT(pool && !deviceId.isNull()))
        return {};

    const auto cameraResource = pool->getResourceById<QnVirtualCameraResource>(deviceId);
    return cameraResource
        ? QnResourceDisplayInfo(cameraResource).toString(Qn::RI_NameOnly)
        : nx::format("<%1>", tr("Removed camera")).toQString();
}

QString BookmarkFacadeStrings::siteEvent()
{
    return tr("Site Event", "Shows that the bookmark was created by a site event");
}

} // namespace nx::vms::common
