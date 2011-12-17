#include "resourceserver.h"



QnAbstractResourceSearcher::QnAbstractResourceSearcher():
m_shouldStop(false),
m_sholudbeUsed(true),
m_localResources(false)
{

}

QnAbstractResourceSearcher::~QnAbstractResourceSearcher()
{

}


void QnAbstractResourceSearcher::setShouldBeUsed(bool use)
{
    m_sholudbeUsed = use;
}


bool QnAbstractResourceSearcher::shouldBeUsed() const
{
    return m_sholudbeUsed;
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


//=============================================================================

void QnAbstractFileResourceSearcher::setPathCheckList(const QStringList& paths)
{
    QMutexLocker locker(&m_mutex);
    m_pathListToCheck = paths;
}

void QnAbstractFileResourceSearcher::clearPathCheckList()
{
    QMutexLocker locker(&m_mutex);
    m_pathListToCheck.clear();
}


QStringList QnAbstractFileResourceSearcher::getPathCheckList() const
{
    QMutexLocker locker(&m_mutex);
    return m_pathListToCheck;
}