#include "resource_pool.h"

Q_GLOBAL_STATIC(QnResourcePool, globalResourcePool)


QnResourcePool::QnResourcePool() :
    m_resourcesMtx(QMutex::Recursive)
{
    qRegisterMetaType<QnResourcePtr>("QnResourcePtr");
    qRegisterMetaType<QnResource::Status>("QnResource::Status");
}

QnResourcePool::~QnResourcePool()
{
}

QnResourcePool *QnResourcePool::instance()
{
    return globalResourcePool();
}

void QnResourcePool::addResource(QnResourcePtr resource)
{
    if (!resource->getId().isValid())
        resource->setId(QnId::generateSpecialId());
    {
        QMutexLocker locker(&m_resourcesMtx);
        m_resources[resource->getId()] = resource;
    }
    Q_EMIT resourceAdded(resource);
}

void QnResourcePool::addResources(const QnResourceList &resources)
{
    foreach (QnResourcePtr resource, resources)
    {
        if (!resource->getId().isValid())
            resource->setId(QnId::generateSpecialId());
    }
    {
        QMutexLocker locker(&m_resourcesMtx);
        foreach (QnResourcePtr resource, resources)
            m_resources[resource->getId()] = resource;
    }
    foreach (QnResourcePtr resource, resources)
        Q_EMIT resourceAdded(resource);
}

void QnResourcePool::removeResource(QnResourcePtr resource)
{
    int removed;
    {
        QMutexLocker locker(&m_resourcesMtx);
        removed = m_resources.remove(resource->getId());
    }
    if (removed != 0)
        Q_EMIT resourceRemoved(resource);
}

void QnResourcePool::removeResources(const QnResourceList &resources)
{
    QnResourceList removedResources;
    {
        QMutexLocker locker(&m_resourcesMtx);
        foreach (QnResourcePtr res, resources)
        {
            if (m_resources.remove(res->getId()) != 0)
                removedResources.append(res);
        }
    }
    foreach (QnResourcePtr res, removedResources)
        Q_EMIT resourceRemoved(res);
}

QnResourcePtr QnResourcePool::getResourceById(const QnId& id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    ResourceMap::const_iterator it = m_resources.constFind(id);
    if (it != m_resources.constEnd())
        return it.value();

    return QnResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUrl(const QString& url) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (QnResourcePtr lres, m_resources)
    {
        if (lres->getUrl() ==  url)
            return lres;
    }

    return QnResourcePtr(0);

}

QnNetworkResourcePtr QnResourcePool::getNetResourceByMac(const QString& mac) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (QnResourcePtr lres, m_resources)
    {
        QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(lres);
        if (netResource != 0 && netResource->getMAC().toString() ==  mac)
            return netResource;
    }

    return QnNetworkResourcePtr(0);

}

QnResourcePtr QnResourcePool::getResourceByUniqId(const QString& id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (QnResourcePtr lres, m_resources)
    {
        if (lres->getUniqueId() ==  id)
            return lres;
    }

    return QnResourcePtr(0);

}

bool QnResourcePool::hasSuchResouce(const QString& uniqid) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (QnResourcePtr lres, m_resources)
    {
        if (lres->getUniqueId() == uniqid)
            return true;
    }

    return false;
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourceList QnResourcePool::getResourcesWithFlag(unsigned long flag)
{
    QnResourceList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (QnResourcePtr res, m_resources)
    {
        if (res->checkFlag(flag))
            result.push_back(res);
    }

    return result;
}


QStringList QnResourcePool::allTags() const
{
    QStringList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (QnResourcePtr resource, m_resources.values())
        result << resource->tagList();

    return result;
}

QnResourceList QnResourcePool::findResourcesByCriteria(const CLDeviceCriteria& cr) const
{
    QnResourceList result;

    if (cr.getCriteria() != CLDeviceCriteria::STATIC && cr.getCriteria() != CLDeviceCriteria::NONE)
    {
        QMutexLocker locker(&m_resourcesMtx);
        foreach (QnResourcePtr resource, m_resources)
        {
            if (isResourceMeetCriteria(cr, resource))
                result.push_back(resource);
        }

    }

    return result;
}


//===============================================================================================


bool QnResourcePool::isResourceMeetCriteria(const CLDeviceCriteria& cr, QnResourcePtr res) const
{
    if (res==0)
        return false;

    if (cr.getCriteria()== CLDeviceCriteria::STATIC)
        return false;

    if (cr.getCriteria()== CLDeviceCriteria::NONE)
        return false;


    if (cr.getCriteria()== CLDeviceCriteria::ALL)
    {
        /*
        if (res->getParentId().isEmpty())
            return false;

        if (cr.getRecorderId() == QLatin1String("*"))
            return true;

        if (res->getParentId() != cr.getRecorderId())
            return false;
            /**/
    }

    if (cr.getCriteria()== CLDeviceCriteria::FILTER)
    {
        if (cr.filter().length()==0)
            return false;

        bool matches = false;

        QStringList serach_list = cr.filter().split(QLatin1Char('+'), QString::SkipEmptyParts);
        foreach (const QString &sub_filter, serach_list)
        {
            if (serach_list.count()<2 || sub_filter.length()>2)
                matches |= match_subfilter(res, sub_filter);
        }

        return matches;
    }

    return true;
}

bool QnResourcePool::match_subfilter(QnResourcePtr resource, const QString& fltr) const
{
    QStringList serach_list = fltr.split(QLatin1Char(' '), QString::SkipEmptyParts);
    foreach(const QString &str, serach_list)
    {
        if (!resource->toString().contains(str, Qt::CaseInsensitive) && !resource->hasTag(str))
            return false;
    }
    return !serach_list.isEmpty();
}
