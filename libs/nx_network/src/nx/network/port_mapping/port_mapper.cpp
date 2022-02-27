// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
