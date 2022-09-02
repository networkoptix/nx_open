// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_snapshot_storage.h"

#include <core/resource/layout_resource.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::client::desktop {

bool LayoutSnapshotStorage::hasSnapshot(const QnLayoutResourcePtr& layout) const
{
    if (!NX_ASSERT(layout, "An attempt to check snapshot availability for null layout"))
        return false;

    return m_snapshotByLayoutId.contains(layout->getId());
}

const nx::vms::api::LayoutData LayoutSnapshotStorage::snapshot(
    const QnLayoutResourcePtr& layout) const
{
    if (!NX_ASSERT(layout, "An attempt to get snapshot for null layout"))
        return {};

    NX_ASSERT(hasSnapshot(layout), "No saved snapshot exists for layout '%1'", layout);

    return m_snapshotByLayoutId.value(layout->getId(), nx::vms::api::LayoutData());
}

void LayoutSnapshotStorage::storeSnapshot(const QnLayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout, "An attempt to store snapshot for null layout"))
        return;

    nx::vms::api::LayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    m_snapshotByLayoutId.insert(layout->getId(), apiLayout);
}

void LayoutSnapshotStorage::removeSnapshot(const QnLayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout, "An attempt to remove snapshot for null layout"))
        return;
        
    m_snapshotByLayoutId.remove(layout->getId());
}

} // namespace nx::vms::client::desktop
