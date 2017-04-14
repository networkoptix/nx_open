#ifndef WORKBENCH_LAYOUT_WATCHER_H
#define WORKBENCH_LAYOUT_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

/**
 * This class can be used for watching the resource pool for layouts that
 * contain local resources that are not in resource pool.
 *
 * Once such resource is inserted into the pool, its children resources are
 * initialized and inserted into the pool.
 */
class QnWorkbenchLayoutWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchLayoutWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchLayoutWatcher();

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
};

#endif // WORKBENCH_LAYOUT_WATCHER_H
