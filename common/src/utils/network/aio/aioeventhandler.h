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

        virtual void eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw() = 0;
    };
}

#endif  //AIOEVENTHANDLER_H
