#include "resourceserver.h"



QnAbstractResourceSearcher::QnAbstractResourceSearcher():
m_shouldStop(false),
m_shouldbeUsed(true),
m_localResources(false)
{

}

QnAbstractResourceSearcher::~QnAbstractResourceSearcher()
{

}


void QnAbstractResourceSearcher::setShouldBeUsed(bool use)
{
    m_shouldbeUsed = use;
}


bool QnAbstractResourceSearcher::shouldBeUsed() const
{
    return m_shouldbeUsed;
}

void QnAbstractResourceSearcher::pleaseStop()
{
    m_shouldStop = true;
}



bool QnAbstractResourceSearcher::shouldStop() const
{
    return m_shouldStop;
}

bool QnAbstractResourceSearcher::isLocal() const
{
    return m_localResources;
}

void QnAbstractResourceSearcher::setLocal(bool l)
{
    m_localResources = l;
}

bool QnAbstractResourceSearcher::isResourceTypeSupported(const QnId& resourceTypeId) const
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
        return false;

    return resourceType->getManufacture() == manufacture();
}


//=============================================================================

QStringList QnAbstractFileResourceSearcher::getPathCheckList() const
{
    QMutexLocker locker(&m_mutex);
    return m_pathListToCheck;
}

void QnAbstractFileResourceSearcher::setPathCheckList(const QStringList& paths)
{
    QMutexLocker locker(&m_mutex);
    m_pathListToCheck = paths;
}

QnResourceList QnAbstractFileResourceSearcher::checkFiles(const QStringList &files) const
{
    QnResourceList result;
    foreach (const QString &file, files) {
        if (QnResourcePtr res = checkFile(file))
            result.append(res);
    }

    return result;
}
