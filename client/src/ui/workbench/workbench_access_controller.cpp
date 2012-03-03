#include "workbench_access_controller.h"
#include <cassert>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_criterion.h>
#include "workbench_context.h"

QnWorkbenchAccessController::QnWorkbenchAccessController(QObject *parent):
    QObject(parent),
    m_context(NULL)
{}

QnWorkbenchAccessController::~QnWorkbenchAccessController() {
    setContext(NULL);
}

QnResourcePool *QnWorkbenchAccessController::resourcePool() const {
    return m_context ? m_context->resourcePool() : NULL;
}

void QnWorkbenchAccessController::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL)
        stop();

    m_context = context;

    if(m_context != NULL)
        start();
}

void QnWorkbenchAccessController::start() {
    assert(context() != NULL);

    connect(context(),      SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_context_aboutToBeDestroyed()));
    connect(context(),      SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(at_context_userChanged(const QnUserResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));

    at_context_userChanged(context()->user());
}

void QnWorkbenchAccessController::stop() {
    assert(context() != NULL);

    disconnect(context(), NULL, this, NULL);
    disconnect(resourcePool(), NULL, this, NULL);
}

bool QnWorkbenchAccessController::isAccessible(const QnUserResourcePtr &user) {
    QnUserResourcePtr currentUser = context()->user();
    if(!currentUser) 
        return false;

    if(user == currentUser)
        return true;

    if(currentUser->isAdmin())
        return true;

    return false;
}

void QnWorkbenchAccessController::updateAccessRights(const QnUserResourcePtr &user) {
    user->setStatus(isAccessible(user) ? QnResource::Online : QnResource::Disabled);
}

void QnWorkbenchAccessController::updateAccessRights(const QnUserResourceList &users) {
    foreach(const QnUserResourcePtr &user, users)
        updateAccessRights(user);
}

void QnWorkbenchAccessController::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnWorkbenchAccessController::at_context_userChanged(const QnUserResourcePtr &) {
    updateAccessRights(QnResourceCriterion::filter<QnUserResource>(resourcePool()->getResources()));
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    updateAccessRights(user);
}

