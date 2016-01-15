/**********************************************************
* Jan 14, 2016
* akolesnikov
***********************************************************/

#pragma once


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
}

typedef int ConnectionMethods;

}   //api
}   //hpm
}   //nx
