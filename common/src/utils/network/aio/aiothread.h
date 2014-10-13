/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOTHREAD_H
#define AIOTHREAD_H

#include <memory>

#include <QtCore/QMutex>

#include <utils/common/long_runnable.h>

#include "aioeventhandler.h"
#include "pollset.h"


namespace aio
{
    template<class SocketType> class AIOThreadImpl;

    /*!
        This class is intended for use only with aio::AIOService
        \todo make it nested in aio::AIOService?
        \note All methods, except for \a pleaseStop(), must be called with \a mutex locked
    */
    template<class SocketType>
    class AIOThread
    :
        public QnLongRunnable
    {
    public:
        /*!
            \param mutex Mutex to use for exclusive access to internal data
        */
        AIOThread( QMutex* const mutex );
        virtual ~AIOThread();

        //!Implementation of QnLongRunnable::pleaseStop
        virtual void pleaseStop() override;

        //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
        /*!
            \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
            \note MUST be called with \a mutex locked
        */
        bool watchSocket(
            SocketType* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler<SocketType>* const eventHandler,
            unsigned int timeoutMS = 0 );
        //!Change timeout of existing polling \a sock for \a eventToWatch to \a timeoutMS. \a eventHandler is changed also
        /*!
            \note If \a sock is not polled, undefined behaviour can occur
        */
        bool changeSocketTimeout(
            SocketType* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler<SocketType>* const eventHandler,
            unsigned int timeoutMS = 0 );
        //!Do not monitor \a sock for event \a eventType
        /*!
            Garantees that no \a eventTriggered will be called after return of this method.
            If \a eventTriggered is running and \a removeFromWatch called not from \a eventTriggered, method blocks till \a eventTriggered had returned
            \param waitForRunningHandlerCompletion See comment to \a aio::AIOService::removeFromWatch
            \return true if removed socket. false if failed to remove
            \note Calling this method with same parameters simultaneously from multiple threads can cause undefined behavour
            \note MUST be called with \a mutex locked
        */
        bool removeFromWatch(
            SocketType* const sock,
            aio::EventType eventType,
            bool waitForRunningHandlerCompletion );
        //!Queues \a functor to be executed from within this aio thread as soon as possible
        bool post( std::function<void()>&& functor );
        //!If called in this aio thread, then calls \a functor immediately, otherwise queues \a functor in same way as \a aio::AIOThread::post does
        bool dispatch( std::function<void()>&& functor );
        //!Returns number of sockets handled by this object
        size_t socketsHandled() const;
        //!Returns true, if can accept socket \a sock for monitoring
        /*!
            \note This method is required only because \a select is used on win32. On linux and mac this method always returns \a true
            \todo remove this method after moving windows implementation to IO Completion Ports
        */
        bool canAcceptSocket( SocketType* const sock ) const;

    protected:
        //!Implementation of QThread::run
        virtual void run() override;

    private:
        typedef AIOThreadImpl<SocketType> AIOThreadImplType;
        AIOThreadImplType* m_impl;
    };
}

#endif  //AIOTHREAD_H
