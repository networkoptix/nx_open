#include "port_mapper.h"

#include <QDateTime>

namespace nx {
namespace network {

PortMapping::PortMapping()
    : requestTime(0), lifeTime(0)
{

}

quint32 PortMapping::timeLeft()
{
    return QDateTime::currentDateTime().toTime_t() + lifeTime - requestTime;
}

} // namespace network
} // namespace nx
