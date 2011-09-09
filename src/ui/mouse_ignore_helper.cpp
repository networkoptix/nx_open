#include "mouse_ignore_helper.h"
#include "base/log.h"



CLMouseIgnoreHelper::CLMouseIgnoreHelper()
{
    reset();
}

CLMouseIgnoreHelper::~CLMouseIgnoreHelper()
{

}

bool CLMouseIgnoreHelper::shouldIgnore() const
{

    if (QDateTime::currentMSecsSinceEpoch() < m_timeUpTo  )
        return true;
    else
        return false;
}

void CLMouseIgnoreHelper::ignoreNextMs(int ms, bool waitTillTheEnd)
{
    ms-=100;

    if (!waitTillTheEnd)
    {
        if (ms > 500)
            ms = 500; // never wait more than 500 ms 
    }

    if (ms<0)
        return;
    
    qint64 candidate = QDateTime::currentMSecsSinceEpoch() + ms;
    if (candidate > m_timeUpTo)
        m_timeUpTo = candidate;
}

void CLMouseIgnoreHelper::reset()
{
    m_timeUpTo = QDateTime::currentMSecsSinceEpoch();
}