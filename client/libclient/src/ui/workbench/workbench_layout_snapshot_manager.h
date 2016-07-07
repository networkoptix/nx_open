#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>

#include <core/resource/resource_fwd.h>

#include <api/abstract_reply_processor.h>

#include <client/client_globals.h>

#include "workbench_context_aware.h"

class QnWorkbenchContext;
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
    bool save(const QnLayoutResourcePtr &resource, SaveLayoutResultFunction callback);

    void store(const QnLayoutResourcePtr &resource);
    void restore(const QnLayoutResourcePtr &resource);

    Qn::ResourceSavingFlags flags(const QnLayoutResourcePtr &resource) const;
    Qn::ResourceSavingFlags flags(QnWorkbenchLayout *layout) const;
    void setFlags(const QnLayoutResourcePtr &resource, Qn::ResourceSavingFlags flags);

    bool isChanged(const QnLayoutResourcePtr &resource) const
    {
        return flags(resource).testFlag(Qn::ResourceIsChanged);
    }

    bool isLocal(const QnLayoutResourcePtr &resource) const
    {
        return flags(resource).testFlag(Qn::ResourceIsLocal);
    }

    bool isSaveable(const QnLayoutResourcePtr &resource) const
    {
        Qn::ResourceSavingFlags flags = this->flags(resource);
        if (flags.testFlag(Qn::ResourceIsBeingSaved))
            return false;

        if (flags & (Qn::ResourceIsLocal | Qn::ResourceIsChanged))
            return true;

        return false;
    }

    bool isModified(const QnLayoutResourcePtr &resource) const {
        return (flags(resource) & (Qn::ResourceIsChanged | Qn::ResourceIsBeingSaved)) == Qn::ResourceIsChanged; /* Changed and not being saved. */
    }

    // TODO: #Elric move out?

signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

    Qn::ResourceSavingFlags defaultFlags(const QnLayoutResourcePtr &resource) const;

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
