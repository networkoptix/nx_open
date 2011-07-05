#include "abstract_resource_searcher.h"


QnAbstractResourceSearcher::QnAbstractResourceSearcher():
m_shouldStop(false)
{

}

QnAbstractResourceSearcher::~QnAbstractResourceSearcher()
{

}


void QnAbstractResourceSearcher::setSholdBeUsed(bool use)
{
    m_sholudbeUsed = use;
}

    
bool QnAbstractResourceSearcher::sholdBeUsed() const
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


//=============================================================================

void QnAbstractFileResourceSearcher::setPathCheckList(const QStringList& paths)
{
    m_pathListToCheck = paths;
}

void QnAbstractFileResourceSearcher::clearPathCheckList()
{
    m_pathListToCheck.clear();
}
