// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/utils/save_state_manager.h>
#include <nx/vms/client/desktop/system_context_aware.h>

class QnWorkbenchLayout;

namespace nx::vms::api { struct LayoutData; }

namespace nx::vms::client::desktop {

class LayoutSnapshotStorage;

/**
 * This class maintains a storage of layout snapshots and tracks the state of
 * each layout.
 *
 * It also provides some functions for layout and snapshot manipulation.
 */
class NX_VMS_CLIENT_DESKTOP_API LayoutSnapshotManager:
    public core::SaveStateManager,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = core::SaveStateManager;

public:
    LayoutSnapshotManager(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~LayoutSnapshotManager();

    typedef std::function<void(bool, const QnLayoutResourcePtr&)> SaveLayoutResultFunction;
    bool save(const QnLayoutResourcePtr& resource,
        SaveLayoutResultFunction callback = SaveLayoutResultFunction());

    bool hasSnapshot(const QnLayoutResourcePtr& layout) const;
    const nx::vms::api::LayoutData snapshot(const QnLayoutResourcePtr& layout) const;

    void store(const QnLayoutResourcePtr& resource);
    void restore(const QnLayoutResourcePtr& resource);

    SaveStateFlags flags(const QnLayoutResourcePtr& resource) const;
    SaveStateFlags flags(QnWorkbenchLayout* layout) const;
    void setFlags(const QnLayoutResourcePtr& resource, SaveStateFlags flags);

    bool isChanged(const QnLayoutResourcePtr& layout) const;
    bool isSaveable(const QnLayoutResourcePtr& layout) const;

    // TODO: #sivanov Naming is not the best.
    /** Whether layout is changed and not is being saved. */
    bool isModified(const QnLayoutResourcePtr& layout) const;

signals:
    void layoutFlagsChanged(const QnLayoutResourcePtr& resource);

private:
    void connectTo(const QnLayoutResourcePtr& resource);
    void disconnectFrom(const QnLayoutResourcePtr& resource);

    void onResourcesAdded(const QnResourceList& resources);
    void onResourcesRemoved(const QnResourceList& resources);
    void at_layout_changed(const QnLayoutResourcePtr& resource);
    void at_resource_changed(const QnResourcePtr& resource);

private:
    /** Layout state storage that this object manages. */
    std::unique_ptr<LayoutSnapshotStorage> m_storage;
};

} // namespace nx::vms::client::desktop
