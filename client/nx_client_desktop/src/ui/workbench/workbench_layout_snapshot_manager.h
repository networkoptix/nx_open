#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>

#include <core/resource/resource_fwd.h>

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
class QnWorkbenchLayoutSnapshotManager: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT;
    typedef Connective<QObject> base_type;

public:
    QnWorkbenchLayoutSnapshotManager(QObject *parent = NULL);
    virtual ~QnWorkbenchLayoutSnapshotManager();

    typedef std::function<void(bool, const QnLayoutResourcePtr &)>  SaveLayoutResultFunction;
    bool save(const QnLayoutResourcePtr &resource, SaveLayoutResultFunction callback = SaveLayoutResultFunction());

    QnWorkbenchLayoutSnapshot snapshot(const QnLayoutResourcePtr &layout) const;

    void store(const QnLayoutResourcePtr &resource);
    void restore(const QnLayoutResourcePtr &resource);

    Qn::ResourceSavingFlags flags(const QnLayoutResourcePtr &resource) const;
    Qn::ResourceSavingFlags flags(QnWorkbenchLayout *layout) const;
    void setFlags(const QnLayoutResourcePtr &resource, Qn::ResourceSavingFlags flags);

    bool isChanged(const QnLayoutResourcePtr &resource) const;
    bool isSaveable(const QnLayoutResourcePtr &resource) const;
    bool isModified(const QnLayoutResourcePtr &resource) const;


signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

protected slots:

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_layout_changed(const QnLayoutResourcePtr &resource);
    void at_layout_itemChanged(const QnLayoutResourcePtr &resource);
    void at_resource_changed(const QnResourcePtr &resource);

private:
    friend class QnWorkbenchLayoutReplyProcessor;

    /** Layout state storage that this object manages. */
    QnWorkbenchLayoutSnapshotStorage *m_storage;

    /** Layout to flags mapping. */
    QHash<QnLayoutResourcePtr, Qn::ResourceSavingFlags> m_flagsByLayout;
};
