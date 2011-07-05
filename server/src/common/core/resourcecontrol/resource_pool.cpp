#include "resource/network_resource.h"
#include "resources/archive/filetypesupport.h"
#include "resource_pool.h"

QnResourcePool::QnResourcePool()
{

}

QnResourcePool::~QnResourcePool()
{

}


QnResourcePool& QnResourcePool::instance()
{
    static QnResourcePool inst;
    return inst;
}




void QnResourcePool::addResource(QnResource* resource)
{
    QMutexLocker mtx(&m_resourcesMtx);
    m_resources[resource->getUniqueId()] = resource;
}

void QnResourcePool::removeResource(QnResource* resource)
{
    QMutexLocker mtx(&m_resourcesMtx);
    m_resources.remove(resource->getUniqueId());

}

QnResource* QnResourcePool::getResourceById(QString id) const
{
    QnResourceList::iterator it = m_resources.find(id);
    if (it == m_resources.end())
        return 0;

    return it.value();
}
