// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_facade.h"

namespace nx::vms::common {

QString BookmarkFacadeBase::creatorName(const QnUuid& creatorId,
    QnResourcePool* resourcePool)
{
    const auto kSystemEventString =
        tr("System Event", "Shows that the bookmark was created by a system event");

    if (creatorId.isNull())
        return {};

    if (creatorId == QnCameraBookmark::systemUserId())
        return kSystemEventString;

    const auto userResource =
        resourcePool->getResourceById<QnUserResource>(creatorId);

    return userResource ? userResource->getName() : QString{};
}

QString BookmarkFacadeBase::cameraName(const QnUuid& cameraId, QnResourcePool* pool)
{
    if (!NX_ASSERT(pool))
        return {};

    const auto cameraResource = pool->getResourceById<QnVirtualCameraResource>(cameraId);
    return cameraResource
        ? QnResourceDisplayInfo(cameraResource).toString(Qn::RI_NameOnly)
        : nx::format("<%1>", tr("Removed camera")).toQString();
}

} // namespace nx::vms::common
