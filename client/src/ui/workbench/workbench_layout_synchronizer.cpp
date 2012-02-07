#include "workbench_layout_synchronizer.h"
#include "workbench.h"

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

void QnWorkbenchLayoutSynchronizer::initUserWorkbench() {
    connect(m_user.data(),  SIGNAL(resourceChanged()),              this, SLOT(at_user_resourceChanged()));

    connect(m_workbench,    SIGNAL(aboutToBeDestroyed()),           this, SLOT(at_workbench_aboutToBeDestroyed()));
    connect(m_workbench,    SIGNAL(layoutsChanged()),               this, SLOT(at_workbench_layoutsChanged()));
    connect(m_workbench,    SIGNAL(itemAdded(QnWorkbenchItem *)),   this, SLOT(at_workbench_layoutsChanged()));
    connect(m_workbench,    SIGNAL(itemRemoved(QnWorkbenchItem *)), this, SLOT(at_workbench_layoutsChanged()));

}

void QnWorkbenchLayoutSynchronizer::deinitUserWorkbench() {
    
}

void QnWorkbenchLayoutSynchronizer::at_user_resourceChanged() {

}

void QnWorkbenchLayoutSynchronizer::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}