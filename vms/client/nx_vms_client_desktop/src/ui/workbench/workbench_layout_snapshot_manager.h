// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/abstract_save_state_manager.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_layout_snapshot.h>

class QnWorkbenchContext;
class QnWorkbenchLayout;
class QnWorkbenchLayoutSnapshotStorage;
class QnWorkbenchLayoutSnapshotManager;

/**
 * This class maintains a storage of layout snapshots and tracks the state of
 * each layout.
 *
 * It also provides some functions for layout and snapshot manipulation.
 */
class NX_VMS_CLIENT_DESKTOP_API QnWorkbenchLayoutSnapshotManager:
    public QnAbstractSaveStateManager,
    public QnCommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT

    using base_type = QnAbstractSaveStateManager;
public:
    QnWorkbenchLayoutSnapshotManager(QObject *parent = nullptr);
    virtual ~QnWorkbenchLayoutSnapshotManager();

    typedef std::function<void(bool, const QnLayoutResourcePtr &)>  SaveLayoutResultFunction;
    bool save(const QnLayoutResourcePtr &resource, SaveLayoutResultFunction callback = SaveLayoutResultFunction());

    bool hasSnapshot(const QnLayoutResourcePtr& layout) const;
    QnWorkbenchLayoutSnapshot snapshot(const QnLayoutResourcePtr &layout) const;

    void store(const QnLayoutResourcePtr &resource);
    void restore(const QnLayoutResourcePtr &resource);

    SaveStateFlags flags(const QnLayoutResourcePtr &resource) const;
    SaveStateFlags flags(QnWorkbenchLayout *layout) const;
    void setFlags(const QnLayoutResourcePtr &resource, SaveStateFlags flags);

    bool isChanged(const QnLayoutResourcePtr& layout) const;
    bool isSaveable(const QnLayoutResourcePtr& layout) const;
    bool isModified(const QnLayoutResourcePtr& layout) const;

signals:
    void layoutFlagsChanged(const QnLayoutResourcePtr &resource);

private:
    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_layout_changed(const QnLayoutResourcePtr &resource);
    void at_resource_changed(const QnResourcePtr &resource);

private:
    friend class QnWorkbenchLayoutReplyProcessor;

    /** Layout state storage that this object manages. */
    std::unique_ptr<QnWorkbenchLayoutSnapshotStorage> m_storage;
};
