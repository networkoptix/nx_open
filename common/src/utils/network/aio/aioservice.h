/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOSERVICE_H
#define AIOSERVICE_H

#include <memory>

#include "aioeventhandler.h"
#include "pollset.h"


namespace aio
{
    class AIOServiceImpl;

    //!Monitors multiple sockets for asynchronous events and triggers handler (\a AIOEventHandler) on event
    /*!
        Holds multiple threads inside, handler triggered from random thread
        \note Internal thread count can be increased dynamically, since this class uses PollSet class which can monitor only limited number of sockets
        \note Suggested use of this class: little add/remove, many notifications
        \note Single socket is always listened for all requested events by the same thread
        \note Currently, win32 implementation uses select, so it is far from being efficient. Linux implementation uses epoll, BSD - kqueue
        \note All methods are thread-safe
        \note All methods are not blocking except \a AIOService::removeFromWatch called with \a waitForRunningHandlerCompletion set to \a true
        \todo Socket termination???
    */
    class AIOService
    {
    public:
        /*!
            After object instanciation one must call \a isInitialized to check whether instanciation was a success
            \param threadCount This is minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
                If zero, thread count is choosed automatically
        */
        AIOService( unsigned int threadCount = 0 );
        virtual ~AIOService();

        //!Returns true, if object has been successfully initialized
        bool isInitialized() const;
        //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
        /*!
            \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
            \note if no event in corresponding socket timeout (if not 0), then aio::etTimedOut event will be reported
            \note If not called from aio thread \a sock can be added to event loop with some delay
        */
        bool watchSocket(
            AbstractSocket* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler* const eventHandler );
        //!Cancel monitoring \a sock for event \a eventType
        /*!
            \param waitForRunningHandlerCompletion \a true garantees that no \a aio::AIOEventHandler::eventTriggered will be called after return of this method
                and all running handlers have returned. But this MAKES METHOD BLOCKING and, as a result, this MUST NOT be called from aio thread
                It is strongly recommended to set this parameter to \a false.

            \note If \a waitForRunningHandlerCompletion is \a false events that are already posted to the queue can be called \b after return of this method
            \note If this method is called from asio thread, \a sock is processed in (e.g., from event handler associated with \a sock),
                this method does not block and always works like \a waitForRunningHandlerCompletion has been set to \a true
        */
        void removeFromWatch(
            AbstractSocket* const sock,
            aio::EventType eventType,
            bool waitForRunningHandlerCompletion = true );
        //!Returns \a true, if socket is still listened for state changes
        bool isSocketBeingWatched(AbstractSocket* sock) const;

        QMutex* mutex() const { return &m_mutex; }

        //!Same as \a AIOService::watchSocket, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
        bool watchSocketNonSafe(
            AbstractSocket* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler* const eventHandler );
        //!Same as \a AIOService::removeFromWatch, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
        void removeFromWatchNonSafe(
            AbstractSocket* const sock,
            aio::EventType eventType,
            bool waitForRunningHandlerCompletion = true );

        /*!
            \param threadCount minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
                If zero, thread count is choosed automatically. This value is used only in first call 
        */
        static AIOService* instance( unsigned int threadCount = 0 );

    private:
        AIOServiceImpl* m_impl;
    };
}

#endif  //AIOSERVICE_H
