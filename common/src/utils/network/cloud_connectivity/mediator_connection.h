/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_CONNECTION_H
#define NX_MEDIATOR_CONNECTION_H

#include <functional>

#include "cc_common.h"


namespace nx_cc
{
    class MediatorMessage
    {
    };

    //!Connection to the mediator
    class MediatorConnection
    {
    public:
        MediatorConnection();
        virtual ~MediatorConnection();

        void registerIndicationHandler( std::function<void(const MediatorMessage&)>&& eventReceiver );
        bool sendRequestAsync(
            const MediatorMessage& request,
            std::function<void(const ErrorDescription&)>&& completionHandler );

        static MediatorConnection* instance();
    };
}

#endif  //NX_MEDIATOR_CONNECTION_H
