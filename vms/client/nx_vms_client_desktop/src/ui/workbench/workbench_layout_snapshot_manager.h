#pragma once

#include <nx_ec/ec_api.h>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/abstract_save_state_manager.h>

#include <api/abstract_reply_processor.h>

#include <client/client_globals.h>

#include <ui/workbench/workbench_layout_snapshot.h>

#include "workbench_context_aware.h"

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
class QnWorkbenchLayoutSnapshotManager:
    public QnAbstractSaveStateManager,
    public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QnAbstractSaveStateManager;
public:
    QnWorkbenchLayoutSnapshotManager(QObject *parent = NULL);
    virtual ~QnWorkbenchLayoutSnapshotManager();

    typedef std::function<void(bool, const QnLayoutResourcePtr &)>  SaveLayoutResultFunction;
    bool save(const QnLayoutResourcePtr &resource, SaveLayoutResultFunction callback = SaveLayoutResultFunction());

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
    void at_layout_itemChanged(const QnLayoutResourcePtr &resource);
    void at_resource_changed(const QnResourcePtr &resource);

private:
    friend class QnWorkbenchLayoutReplyProcessor;

    /** Layout state storage that this object manages. */
    QnWorkbenchLayoutSnapshotStorage *m_storage;
};
