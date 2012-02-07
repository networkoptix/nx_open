#include "workbench_layout_synchronizer.h"
#include <core/resourcemanagment/resource_pool.h>
#include <utils/common/scoped_value_rollback.h>
#include "workbench.h"
#include "workbench_layout.h"

QnWorkbenchLayoutSynchronizer::QnWorkbenchLayoutSynchronizer(QObject *parent):
    QObject(parent),
    m_submit(false),
    m_update(false)
{}

QnWorkbenchLayoutSynchronizer::~QnWorkbenchLayoutSynchronizer() {
    setWorkbench(NULL);
    setUser(QnUserResourcePtr());
}

void QnWorkbenchLayoutSynchronizer::setWorkbench(QnWorkbench *workbench) {
    if(m_workbench == workbench)
        return;

    if(m_workbench != NULL && !m_user.isNull())
        deinitUserWorkbench();

    m_workbench = workbench;

    if(m_workbench != NULL && !m_user.isNull())
        initUserWorkbench();
}

void QnWorkbenchLayoutSynchronizer::setUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    if(m_workbench != NULL && !m_user.isNull())
        deinitUserWorkbench();

    m_user = user;

    if(m_workbench != NULL && !m_user.isNull())
        initUserWorkbench();
}

void QnWorkbenchLayoutSynchronizer::addLayoutResource(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource) {
    assert(layout != NULL && !resource.isNull());
    assert(!m_resourceByLayout.contains(layout));
    assert(!m_layoutByResource.contains(resource));

    m_layoutByResource[resource] = layout;
    m_resourceByLayout[layout] = resource;

    connect(layout,             SIGNAL(itemAdded(QnWorkbenchItem *)),       this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(layout,             SIGNAL(itemRemoved(QnWorkbenchItem *)),     this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
    connect(layout,             SIGNAL(nameChanged()),                      this, SLOT(at_layout_nameChanged()));
    connect(resource.data(),    SIGNAL(resourceChanged()),                  this, SLOT(at_layout_resourceChanged()));
}

void QnWorkbenchLayoutSynchronizer::removeLayoutResource(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource) {
    assert(layout != NULL && !resource.isNull());
    assert(m_resourceByLayout.contains(layout));
    assert(m_layoutByResource.contains(resource));

    m_layoutByResource.remove(resource);
    m_resourceByLayout.remove(layout);
}

void QnWorkbenchLayoutSynchronizer::initUserWorkbench() {
    connect(m_user.data(),      SIGNAL(resourceChanged()),                  this, SLOT(at_user_resourceChanged()));

    connect(m_workbench,        SIGNAL(aboutToBeDestroyed()),               this, SLOT(at_workbench_aboutToBeDestroyed()));
    connect(m_workbench,        SIGNAL(layoutsChanged()),                   this, SLOT(at_workbench_layoutsChanged()));
    connect(m_workbench,        SIGNAL(currentLayoutAboutToBeChanged()),    this, SLOT(at_workbench_currentLayoutAboutToBeChanged()));

    m_submit = m_update = true;
}

void QnWorkbenchLayoutSynchronizer::deinitUserWorkbench() {
    m_submit = m_update = false;
    

}

// -------------------------------------------------------------------------- //
// User / Workbench handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSynchronizer::at_user_resourceChanged() {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QSet<QnLayoutDataPtr> resources = m_user->getLayouts().toSet();

    /* New layouts may have been added, but these are not on the workbench, 
     * so we don't need to do anything about them. 
     * 
     * Layouts may have been removed, and in this case we need to remove them
     * from the workbench too. */
    foreach(QnWorkbenchLayout *layout, m_workbench->layouts()) {
        QnLayoutDataPtr resource = this->resource(layout);

        if(!resources.contains(resource))
            delete layout;
    }
}

void QnWorkbenchLayoutSynchronizer::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}

void QnWorkbenchLayoutSynchronizer::at_workbench_layoutsChanged() {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);

    /* Layout may have been closed, but it doesn't mean that we should remove it.
     *
     * New layout may have been added, and in this case we need to create a new'
     * resource for it. */
    foreach(QnWorkbenchLayout *layout, m_workbench->layouts()) {
        QnLayoutDataPtr resource = this->resource(layout);
        if(resource.isNull()) {
            QnLayoutDataPtr resource(new QnLayoutData());
            qnResPool->addResource(resource);

            m_user->addLayout(resource);

            addLayoutResource(layout, resource);
            submitLayout(layout, resource);
        }
    }
}


// -------------------------------------------------------------------------- //
// Layout handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSynchronizer::updateLayout(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource) {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);
    layout->load(resource);
}

void QnWorkbenchLayoutSynchronizer::submitLayout(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource) {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);
    layout->save(resource);
}

void QnWorkbenchLayoutSynchronizer::at_layout_resourceChanged() {
    if(!m_update)
        return;
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemAdded(QnWorkbenchItem *item) {

}

void QnWorkbenchLayoutSynchronizer::at_layout_itemRemoved(QnWorkbenchItem *item) {

}

void QnWorkbenchLayoutSynchronizer::at_layout_nameChanged() {

}