#include "ptz_controller_pool.h"

// TODO: #Elric managment? rename to managEment
#include <core/resource_managment/resource_pool.h>

QnPtzControllerPool::QnPtzControllerPool(QObject *parent):
    base_type(parent)
{
    QnResourcePool *resourcePool = qnResPool;
    connect(resourcePool, &QnResourcePool::resourceAdded,   this,   &QnPtzControllerPool::registerResource,     Qt::QueuedConnection);
    connect(resourcePool, &QnResourcePool::resourceRemoved, this,   &QnPtzControllerPool::unregisterResource,   Qt::QueuedConnection);
    foreach(const QnResourcePtr &resource, resourcePool->getResources())
        registerResource(resource);
}

QnPtzControllerPool::~QnPtzControllerPool() {
    return;
}

QnPtzControllerPtr QnPtzControllerPool::controller(const QnResourcePtr &resource) const {
    return m_controllerByResource.value(resource, QnPtzControllerPtr());
}

void QnPtzControllerPool::setController(const QnResourcePtr &resource, const QnPtzControllerPtr &controller) {
    if(controller == this->controller(resource))
        return;

    if(!controller) {
        m_controllerByResource.remove(resource);
    } else {
        m_controllerByResource.insert(resource, controller);
    }

    emit controllerChanged(resource);
}

void QnPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    setController(resource, createController(resource));
}

void QnPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    setController(resource, QnPtzControllerPtr());
}

QnPtzControllerPtr QnPtzControllerPool::createController(const QnResourcePtr &resource) {
    return QnPtzControllerPtr();
}

void QnPtzControllerPool::updateController(const QnResourcePtr &resource) {
    setController(resource, createController(resource));
}
