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


void QnResourcePool::addResource(QnResourcePtr resource)
{
    QMutexLocker mtx(&m_resourcesMtx);
    m_resources[resource->getId()] = resource;
}

void QnResourcePool::removeResource(QnResourcePtr resource)
{
    QMutexLocker mtx(&m_resourcesMtx);
    m_resources.remove(resource->getId());

}

QnResourcePtr QnResourcePool::getResourceById(const QString& id) const
{
    ResourceMap::const_iterator it = m_resources.find(id);
    if (it == m_resources.end())
        return QnResourcePtr(0);

    return it.value();
}

QnUrlResourcePtr QnResourcePool::getResourceByUrl(const QString& url) const
{
    for (ResourceMap::const_iterator it = m_resources.constBegin(); it != m_resources.constEnd(); ++it)
    {
        QnUrlResourcePtr mr = it->dynamicCast<QnURLResource>();
        if (mr != 0 && mr->getUrl() == url)
            return mr;
    }
    return QnUrlResourcePtr(0);
}


QnResourcePtr QnResourcePool::hasEqualResource(QnResourcePtr res) const
{
    QMutexLocker mtx(&m_resourcesMtx);
    foreach(QnResourcePtr lres, m_resources)
    {
        if (lres->equalsTo(res))
            return lres;
    }

    return QnResourcePtr(0);
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker mtx(&m_resourcesMtx);
    return m_resources.values();
}

QnResourceList QnResourcePool::getResourcesWithFlag(unsigned long flag)
{
    QnResourceList result;
    QMutexLocker mtx(&m_resourcesMtx);

    foreach(QnResourcePtr res, m_resources)
    {
        if (res->checkFlag(flag))
            result.push_back(res);
    }

    return result;
}