/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_STUN_CONNECTION_H
#define NX_STUN_CONNECTION_H

#if 0

#include <functional>

#include "stun_message.h"


namespace nx_stun
{
    //!Connection to STUN server (client-side)
    class ClientConnection
    {
    public:
        void registerIndicationHandler( std::function<void(const nx_stun::Message&)>&& eventReceiver );
        /*!
            \return \a true if async request has been started, \a false otherwise
        */
        bool sendRequestAsync(
            const StunMessage& request,
            std::function<void(nx_stun::attr::ErrorCode, const nx_stun::Message&)>&& completionHandler );
    };
}

#endif

#endif  //NX_STUN_CONNECTION_H
