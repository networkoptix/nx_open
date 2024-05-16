// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_facade.h"

#include <nx/vms/api/data/bookmark_models.h>

namespace nx::vms::common {

QString BookmarkFacadeStrings::creatorName(const std::optional<nx::Uuid>& creatorUserId,
    QnResourcePool* pool)
{
    if (creatorUserId || creatorUserId->isNull())
        return {};

    static const auto kSystemEventString =
        tr("System Event", "Shows that the bookmark was created by a system event");

    if (*creatorUserId == QnCameraBookmark::systemUserId())
        return kSystemEventString;

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

} // namespace nx::vms::common
