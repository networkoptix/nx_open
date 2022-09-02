// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/layout_data.h>

namespace nx::vms::client::desktop {

/**
 * This class provides easy access to snapshots of workbench layouts.
 */
class LayoutSnapshotStorage
{
public:
    LayoutSnapshotStorage() = default;

    bool hasSnapshot(const QnLayoutResourcePtr& layout) const;
    const nx::vms::api::LayoutData snapshot(const QnLayoutResourcePtr& layout) const;
    void storeSnapshot(const QnLayoutResourcePtr& layout);
    void removeSnapshot(const QnLayoutResourcePtr& layout);

private:
    QHash<QnUuid, nx::vms::api::LayoutData> m_snapshotByLayoutId;
};

} // namespace nx::vms::client::desktop
