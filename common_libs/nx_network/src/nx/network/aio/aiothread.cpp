/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aiothread.h"



//TODO #ak memory order semantic used with std::atomic
//TODO #ak move task queues to socket for optimization
//TODO #ak should add some sequence which will be incremented when socket is started polling on any event
    //this is required to distinguish tasks. E.g., socket is polled, than polling stopped asynchronously, 
    //before async stop completion socket is polled again. In this case remove task should not remove tasks 
    //from new polling "session"? Something needs to be done about it


namespace nx {
namespace network {
namespace aio {

template class AIOThread<Pollable>;
//template class AIOThread<UdtSocket>;

}   //aio
}   //network
}   //nx
