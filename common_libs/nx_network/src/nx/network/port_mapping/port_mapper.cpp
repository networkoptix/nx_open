#include "port_mapper.h"

#include <QDateTime>

PortMapping::PortMapping()
    : requestTime(0), lifeTime(0)
{

}

quint32 PortMapping::timeLeft()
{
    return QDateTime::currentDateTime().toTime_t() + lifeTime - requestTime;
}
