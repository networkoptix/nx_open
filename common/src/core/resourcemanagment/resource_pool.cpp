#include "resource_pool.h"

#include <QtCore/QMetaObject>

#include "utils/common/warnings.h"
#include "utils/common/checked_cast.h"
#include "core/resource/video_server.h"

Q_GLOBAL_STATIC(QnResourcePool, globalResourcePool)

QnResourcePool::QnResourcePool() : QObject(),
    m_resourcesMtx(QMutex::Recursive)
{
    qRegisterMetaType<QnResourcePtr>("QnResourcePtr");
    qRegisterMetaType<QnResourceList>("QnResourceList");
    qRegisterMetaType<QnResource::Status>("QnResource::Status");

    localServer = QnResourcePtr(new QnLocalVideoServerResource);
    addResource(localServer);
}

QnResourcePool::~QnResourcePool()
{
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    QMutexLocker locker(&m_resourcesMtx);
    m_resources.clear();
}

QnResourcePool *QnResourcePool::instance()
{
    return globalResourcePool();
}

void QnResourcePool::addResources(const QnResourceList &resources)
{
    QMutexLocker locker(&m_resourcesMtx);

    foreach (const QnResourcePtr &resource, resources) 
    {
        if (!resource->toSharedPointer()) 
        {
            qnWarning("Resource '%1' does not have an assiciated shared pointer. Did you forget to use QnSharedResourcePointer?", resource->metaObject()->className());
            QnSharedResourcePointer<QnResource>::initialize(resource);
        }

        // if resources are local assign localserver as parent
        if (!resource->getParentId().isValid()) 
        {
            if (resource->checkFlags(QnResource::local))
                resource->setParentId(localServer->getId());
        }

        if (!resource->getId().isValid()) 
        {
            // must be just found local resource; => shold not be in the pool already 

            if (QnResourcePtr existing = getResourceByUniqId(resource->getUniqueId())) 
            {
                qnWarning("Resource with UID '%1' is already in the pool. Expect troubles.",
                          resource->getUniqueId().toLocal8Bit().constData());

                resource->setId(existing->getId());

                //Q_ASSERT(false); //new resource in the pool already? strange case; please pay attantion
            } 
            else 
            {
                resource->setId(QnId::generateSpecialId());
            }
        }

    }

    QMap<QnId, QnResourcePtr> newResources; // sort by id

    foreach (const QnResourcePtr &resource, resources) 
    {
        const QnId resId = resource->getId();
        QString uniqueId = resource->getUniqueId();

        if (m_resources.contains(uniqueId)) 
        {
            // if we already have such resource in the pool
            m_resources[uniqueId]->update(resource);
        }
        else
        {
            // new resource 
            m_resources[uniqueId] = resource;
            m_resourceTree[resource->getParentId()].append(resource); // unsorted
            newResources.insert(resId, resource);
        }

    }

    foreach (const QnResourcePtr &resource, newResources.values()) 
    {
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleResourceChange()));
        connect(resource.data(), SIGNAL(resourceChanged()), this, SLOT(handleResourceChange()));
        QMetaObject::invokeMethod(this, "resourceAdded", Qt::QueuedConnection, Q_ARG(QnResourcePtr, resource));
    }

}

void QnResourcePool::removeResources(const QnResourceList &resources)
{
    QnResourceList removedResources;

    QMutexLocker locker(&m_resourcesMtx);

    foreach (const QnResourcePtr &resource, resources) 
    {
        if (resource == localServer) // special case
            continue;

        QString uniqueId = resource->getUniqueId();

        if (m_resources.remove(uniqueId) != 0) 
        {
            m_resourceTree[resource->getParentId()].removeOne(resource);
            removedResources.append(resource);
        }
    }

    foreach (const QnResourcePtr &resource, removedResources) {
        disconnect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleResourceChange()));

        QMetaObject::invokeMethod(this, "resourceRemoved", Qt::QueuedConnection, Q_ARG(QnResourcePtr, resource));
    }
}

void QnResourcePool::handleResourceChange()
{
    const QnResourcePtr resource = toSharedPointer(checked_cast<QnResource *>(sender()));

    QMetaObject::invokeMethod(this, "resourceChanged", Qt::QueuedConnection, Q_ARG(QnResourcePtr, resource));
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(QnId id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach(const QnResourcePtr& res, m_resources)
    {
        if (res->getId() == id)
            return res;
    }

    return QnResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUrl(const QString &url) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        if (resource->getUrl() == url)
            return resource;
    }

    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnResourcePool::getNetResourceByMac(const QString &mac) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getMAC().toString() == mac)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUniqId(const QString &id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    QHash<QString, QnResourcePtr>::const_iterator itr = m_resources.find(id);
    return itr != m_resources.end() ? itr.value() : QnResourcePtr(0);
}

bool QnResourcePool::hasSuchResouce(const QString &uniqid) const
{
    return !getResourceByUniqId(uniqid).isNull();
}

QnResourceList QnResourcePool::getResourcesWithFlag(QnResource::Flag flag) const
{
    QnResourceList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        if (resource->checkFlags(flag))
            result.append(resource);
    }

    return result;
}

QnResourceList QnResourcePool::getResourcesWithParentId(QnId id) const
{
    return m_resourceTree.value(id);
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
