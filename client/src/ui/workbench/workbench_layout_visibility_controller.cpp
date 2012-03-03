#include "workbench_layout_visibility_controller.h"
#include <cassert>
#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

QnWorkbenchLayoutVisibilityController::QnWorkbenchLayoutVisibilityController(QObject *parent):
    QObject(parent),
    m_context(NULL)
{}

QnWorkbenchLayoutVisibilityController::~QnWorkbenchLayoutVisibilityController() {
    setContext(NULL);
}

void QnWorkbenchLayoutVisibilityController::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL)
        stop();

    m_context = context;

    if(m_context != NULL)
        start();
}

QnWorkbenchLayoutSnapshotManager *QnWorkbenchLayoutVisibilityController::snapshotManager() const {
    return m_context ? m_context->snapshotManager() : NULL;
}

void QnWorkbenchLayoutVisibilityController::start() {
    assert(m_context != NULL);

    connect(context(),          SIGNAL(aboutToBeDestroyed()),                       this, SLOT(at_context_aboutToBeDestroyed()));
    connect(snapshotManager(),  SIGNAL(flagsChanged(const QnLayoutResourcePtr &)),  this, SLOT(at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &)));
}

void QnWorkbenchLayoutVisibilityController::stop() {
    assert(m_context != NULL);

    disconnect(context(), NULL, this, NULL);
    disconnect(snapshotManager(), NULL, this, NULL);
}

void QnWorkbenchLayoutVisibilityController::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnWorkbenchLayoutVisibilityController::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    Qn::LayoutFlags flags = snapshotManager()->flags(resource);

    /* Hide local layouts. */
    resource->setStatus(flags & Qn::LayoutIsLocal ? QnResource::Disabled : QnResource::Online);
}


