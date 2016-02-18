/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#include "pollset.h"


namespace nx {
namespace network {
namespace aio {

    const char* toString( EventType eventType )
    {
        switch( eventType )
        {
            case etNone:
                return "etNone";
            case etRead:
                return "etRead";
            case etWrite:
                return "etWrite";
            case etError:
                return "etError";
            case etTimedOut:
                return "etTimedOut";
            default:
                return "unknown";
        }
    }

}   //aio
}   //network
}   //nx
