/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOEVENTHANDLER_H
#define AIOEVENTHANDLER_H

#include <QSharedPointer>

#include "pollset.h"
#include "../socket.h"


namespace aio
{
    class AIOEventHandler
    {
    public:
        virtual ~AIOEventHandler() {}

        //!Receives socket state change event
        /*!
            Implementation MUST NOT block otherwise it will result poor performance
        */
        virtual void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw() = 0;
    };
}

#endif  //AIOEVENTHANDLER_H
