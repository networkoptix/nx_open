#include "mouse_ignore_helper.h"
#include "base/log.h"


CLMouseIgnoreHelper::CLMouseIgnoreHelper()
{
    m_timeUpTo.restart();
}

CLMouseIgnoreHelper::~CLMouseIgnoreHelper()
{

}

bool CLMouseIgnoreHelper::shouldIgnore() const
{
    if (QTime::currentTime() < m_timeUpTo)
    {
        //
        return true;
    }
    else
    {
        //
        return false;
    }
}

void CLMouseIgnoreHelper::ignoreNextMs(int ms)
{
    ms-=100;
    if (ms<0)
        return;
    
    QTime candidate = QTime::currentTime().addMSecs(ms);
    if (candidate > m_timeUpTo)
        m_timeUpTo = candidate;
}

void CLMouseIgnoreHelper::reset()
{
    m_timeUpTo = QTime::currentTime();
}