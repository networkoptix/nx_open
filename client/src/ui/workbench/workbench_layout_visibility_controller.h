#ifndef QN_WORKBENCH_LAYOUT_VISIBILITY_CONTROLLER_H
#define QN_WORKBENCH_LAYOUT_VISIBILITY_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include "workbench_context_aware.h"

class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotManager;

/**
 * This class implements hiding of layouts that are not saved to application server. 
 */
class QnWorkbenchLayoutVisibilityController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchLayoutVisibilityController(QObject *parent = NULL);

    virtual ~QnWorkbenchLayoutVisibilityController();

protected slots:
    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource);
};

#endif // QN_WORKBENCH_LAYOUT_VISIBILITY_CONTROLLER_H
