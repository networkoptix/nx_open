#include "workbench_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include "workbench.h"
#include "workbench_layout.h"
#include "workbench_layout_synchronizer.h"

QnWorkbenchSynchronizer::QnWorkbenchSynchronizer(QObject *parent):
    QObject(NULL),
    m_workbench(NULL),
    m_update(false),
    m_submit(false)
{}

QnWorkbenchSynchronizer::~QnWorkbenchSynchronizer() {}

void QnWorkbenchSynchronizer::setWorkbench(QnWorkbench *workbench) {
    if(m_workbench == workbench)
        return;

    if(m_workbench != NULL && !m_user.isNull())
        deinitialize();

    m_workbench = workbench;

    if(m_workbench != NULL && !m_user.isNull())
        initialize();
}

void QnWorkbenchSynchronizer::setUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    if(m_workbench != NULL && !m_user.isNull())
        deinitialize();

    m_user = user;

    if(m_workbench != NULL && !m_user.isNull())
        initialize();
}

void QnWorkbenchSynchronizer::initialize() {
    assert(m_workbench != NULL && !m_user.isNull());

    /* Clean workbench's layouts. */
    while(!m_workbench->layouts().isEmpty())
        delete m_workbench->layouts().back();

    /* Start listening to changes. */
    connect(m_user.data(),      SIGNAL(resourceChanged()),                  this, SLOT(at_user_resourceChanged()));

    connect(m_workbench,        SIGNAL(aboutToBeDestroyed()),               this, SLOT(at_workbench_aboutToBeDestroyed()));
    connect(m_workbench,        SIGNAL(layoutsChanged()),                   this, SLOT(at_workbench_layoutsChanged()));
    connect(m_workbench,        SIGNAL(currentLayoutAboutToBeChanged()),    this, SLOT(at_workbench_currentLayoutAboutToBeChanged()));

    m_submit = m_update = true;
}

void QnWorkbenchSynchronizer::deinitialize() {
    assert(m_workbench != NULL && !m_user.isNull());

    m_submit = m_update = false;

    /* Stop listening to changes. */
    disconnect(m_workbench, NULL, this, NULL);
    disconnect(m_user.data(), NULL, this, NULL);

    /* Clean workbench's layouts. */
    while(!m_workbench->layouts().isEmpty())
        delete m_workbench->layouts().back();
}

QnLayoutResourcePtr QnWorkbenchSynchronizer::resource(QnWorkbenchLayout *layout) const {
    QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(layout);
    if(synchronizer == NULL)
        return QnLayoutResourcePtr();

    return synchronizer->resource();
}

QnWorkbenchLayout *QnWorkbenchSynchronizer::layout(const QnLayoutResourcePtr &resource) const {
    QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource);
    if(synchronizer == NULL)
        return NULL;

    return synchronizer->layout();
}

void QnWorkbenchSynchronizer::update() {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QSet<QnLayoutResourcePtr> resources = m_user->getLayouts().toSet();

    /* New layouts may have been added, but these are not on the workbench, 
     * so we don't need to do anything about them. 
     * 
     * Layouts may have been removed, and in this case we need to remove them
     * from the workbench too. */
    foreach(QnWorkbenchLayout *layout, m_workbench->layouts()) {
        QnLayoutResourcePtr resource = this->resource(layout);

        if(!resources.contains(resource)) /* Corresponding layout resource was removed, remove layout. */
            delete layout;
    }
}

void QnWorkbenchSynchronizer::submit() {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);

    /* Layout may have been closed, but it doesn't mean that we should remove it.
     *
     * New layout may have been added, and in this case we need to create a new
     * resource for it. */
    foreach(QnWorkbenchLayout *layout, m_workbench->layouts()) {
        QnLayoutResourcePtr resource = this->resource(layout);

        if(resource.isNull()) { 
            /* This actually is a newly created layout. */
            resource = QnLayoutResourcePtr(new QnLayoutResource());
            qnResPool->addResource(resource);
            m_user->addLayout(resource);

            QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(layout, resource, this);
            synchronizer->setAutoDeleting(true);
            synchronizer->submit();
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchSynchronizer::at_user_resourceChanged() {
    update();
}

void QnWorkbenchSynchronizer::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}

void QnWorkbenchSynchronizer::at_workbench_layoutsChanged() {
    submit();
}
