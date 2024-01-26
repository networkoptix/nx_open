// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <core/resource/videowall_resource.h>
#include <utils/screen_snaps_geometry.h>
#include <utils/screen_utils.h>

namespace nx::vms::client::desktop {

QSet<int> screensCoveredByItem(const QnVideoWallItem& item, const QnVideoWallResourcePtr& videoWall)
{
    QList<QRect> screenGeometries;
    if (NX_ASSERT(videoWall))
    {
        for (const auto& screen: videoWall->pcs()->getItem(item.pcUuid).screens)
            screenGeometries << screen.desktopGeometry;
    }

    const QRect itemGeometry = screenSnapsGeometry(item.screenSnaps, screenGeometries);
    return nx::gui::Screens::coveredBy(itemGeometry, screenGeometries);
}

} // namespace nx::vms::client::desktop
