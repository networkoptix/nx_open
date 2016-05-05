/**********************************************************
* Jan 14, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/log/log_message.h>

namespace nx {
namespace hpm {
namespace api {

namespace ConnectionMethod
{
    enum Value
    {
        udpHolePunching = 1,
        tcpHolePunching = 2,
        proxy = 4,
        reverseConnect = 8,
    };

    inline QString toString(int value)
    {
        QStringList list;
        if (value & udpHolePunching) list << QLatin1String("udpHolePunching");
        if (value & tcpHolePunching) list << QLatin1String("tcpHolePunching");
        if (value & proxy) list << QLatin1String("proxy");
        if (value & reverseConnect) list << QLatin1String("reverseConnect");
        return containerString(list);
    }
}

typedef int ConnectionMethods;

}   //api
}   //hpm
}   //nx
