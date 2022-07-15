// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_snapshot_storage.h"

#include <core/resource/layout_resource.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::client::desktop {

LayoutSnapshotStorage::LayoutSnapshotStorage(QObject* parent):
    QObject(parent)
{
}

LayoutSnapshotStorage::~LayoutSnapshotStorage()
{
}

const nx::vms::api::LayoutData& LayoutSnapshotStorage::snapshot(
    const QnLayoutResourcePtr& layout) const
{
    static nx::vms::api::LayoutData emptyResult;

    auto pos = m_snapshotByLayoutId.find(layout->getId());
    if (pos == m_snapshotByLayoutId.end())
    {
        NX_ASSERT(false, "No saved snapshot exists for layout '%1'.", layout);
        return emptyResult;
    }

    return *pos;
}

void LayoutSnapshotStorage::store(const QnLayoutResourcePtr& layout)
{
    nx::vms::api::LayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    m_snapshotByLayoutId[layout->getId()] = apiLayout;
}

void LayoutSnapshotStorage::remove(const QnLayoutResourcePtr& layout)
{
    m_snapshotByLayoutId.remove(layout->getId());
}

void LayoutSnapshotStorage::clear()
{
    m_snapshotByLayoutId.clear();
}

} // namespace nx::vms::client::desktop
