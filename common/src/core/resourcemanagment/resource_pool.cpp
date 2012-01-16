#include "resource_pool.h"

#include <QtCore/QMetaObject>

#include "core/resource/video_server.h"

Q_GLOBAL_STATIC(QnResourcePool, globalResourcePool)

QnResourcePool::QnResourcePool() : QObject(),
    m_resourcesMtx(QMutex::Recursive)
{
    qRegisterMetaType<QnResourcePtr>("QnResourcePtr");
    qRegisterMetaType<QnResourceList>("QnResourceList");
    qRegisterMetaType<QnResource::Status>("QnResource::Status");

    addResource(QnResourcePtr(new QnLocalVideoServer));
}

QnResourcePool *QnResourcePool::instance()
{
    return globalResourcePool();
}

void QnResourcePool::addResources(const QnResourceList &resources)
{
    ResourceMap newResources; // sort by id

    QMutexLocker locker(&m_resourcesMtx);

    foreach (const QnResourcePtr &resource, resources)
    {
        if (!resource->getId().isValid())
            resource->setId(QnId::generateSpecialId());
    }

    foreach (const QnResourcePtr &resource, resources)
    {
        if ((resource->flags() & (QnResource::local | QnResource::remote)) == 0) {
            qWarning("QnResourcePool::addResources(): invalid resource has been detected (nor local neither remote)");
            continue; // ignore
        }

        const QnId &resId = resource->getId();
        if (!m_resources.contains(resId))
        {
            m_resources.insert(resId, resource);
            newResources.insert(resId, resource);
        }
    }

    foreach (const QnResourcePtr &resource, newResources.values()) {
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleResourceChange()));

        QMetaObject::invokeMethod(this, "resourceAdded", Qt::QueuedConnection, Q_ARG(QnResourcePtr, resource));
    }
}

void QnResourcePool::removeResources(const QnResourceList &resources)
{
    QnResourceList removedResources;

    QMutexLocker locker(&m_resourcesMtx);

    foreach (const QnResourcePtr &resource, resources)
    {
        if (m_resources.remove(resource->getId()) != 0)
            removedResources.append(resource);
    }

    foreach (const QnResourcePtr &resource, removedResources) {
        disconnect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleResourceChange()));

        QMetaObject::invokeMethod(this, "resourceRemoved", Qt::QueuedConnection, Q_ARG(QnResourcePtr, resource));
    }
}

void QnResourcePool::handleResourceChange()
{
    const QnResourcePtr resource = qobject_cast<QnResource *>(sender())->toSharedPointer();
    Q_EMIT resourceChanged(resource);
}

QnResourcePtr QnResourcePool::getResourceById(const QnId &id) const
{
    if (id.isValid())
    {
        QMutexLocker locker(&m_resourcesMtx);
        ResourceMap::const_iterator it = m_resources.constFind(id);
        if (it != m_resources.constEnd())
            return it.value();
    }

    return QnResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUrl(const QString &url) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
    {
        if (resource->getUrl() == url)
            return resource;
    }

    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnResourcePool::getNetResourceByMac(const QString &mac) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
    {
        QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
        if (netResource != 0 && netResource->getMAC().toString() == mac)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUniqId(const QString &id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
    {
        if (resource->getUniqueId() == id)
            return resource;
    }

    return QnResourcePtr(0);
}

bool QnResourcePool::hasSuchResouce(const QString &uniqid) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
    {
        if (resource->getUniqueId() == uniqid)
            return true;
    }

    return false;
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourceList QnResourcePool::getResourcesWithFlag(unsigned long flag) const
{
    QnResourceList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
    {
        if (resource->checkFlag(flag))
            result.append(resource);
    }

    return result;
}

QnResourceList QnResourcePool::getResourcesWithParentId(const QnId &id) const
{
    QnResourceList result;

    if (id.isValid())
    {
        QMutexLocker locker(&m_resourcesMtx);
        foreach (const QnResourcePtr &resource, m_resources)
        {
            if (resource->getParentId() == id)
                result.append(resource);
        }
    }

    return result;
}


QStringList QnResourcePool::allTags() const
{
    QStringList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources.values())
        result << resource->tagList();

    return result;
}

QnResourceList QnResourcePool::findResourcesByCriteria(const CLDeviceCriteria& cr) const
{
    QnResourceList result;

    if (cr.getCriteria() != CLDeviceCriteria::STATIC && cr.getCriteria() != CLDeviceCriteria::NONE)
    {
        QMutexLocker locker(&m_resourcesMtx);
        foreach (const QnResourcePtr &resource, m_resources)
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
