#include "workbench_layout_visibility_controller.h"
#include <cassert>
#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

QnWorkbenchLayoutVisibilityController::QnWorkbenchLayoutVisibilityController(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(snapshotManager(),  SIGNAL(flagsChanged(const QnLayoutResourcePtr &)),  this, SLOT(at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &)));
}

QnWorkbenchLayoutVisibilityController::~QnWorkbenchLayoutVisibilityController() {
    disconnect(snapshotManager(), NULL, this, NULL);
}

void QnWorkbenchLayoutVisibilityController::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    Qn::LayoutFlags flags = snapshotManager()->flags(resource);

    /* Hide local layouts. */
    resource->setStatus(flags & Qn::LayoutIsLocal ? QnResource::Disabled : QnResource::Online);
}


