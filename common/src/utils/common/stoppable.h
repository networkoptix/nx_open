/**********************************************************
* 28 jan 2013
* a.kolesnikov
***********************************************************/

#ifndef QNSTOPPABLE_H
#define QNSTOPPABLE_H

#include <functional>
#include <future>
#include <vector>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/barrier_handler.h>


/** Abstract class providing interface to stop doing anything without object destruction */
class QN_EXPORT QnStoppable
{
public:
    virtual ~QnStoppable() {}

    virtual void pleaseStop() = 0;
};

/** Abstract interface to interrupt asynchronous operation with completion notification */
class QN_EXPORT QnStoppableAsync
{
public:
    virtual ~QnStoppableAsync() {}

    /** Ask object to interrupt all asynchoronous operations.
        Caller MUST ensure that no asynchronous operations are started after this call
        \param completionHandler Executed when asynchronous operation is interrupted. For example, in case with async socket operations, 
            \a completionHandler is triggered when socket completion handler returned or will never be called.
            Allowed to be \a null
        \note If operation is already stopped it is allowed for \a completionHandler to be executed directly in \a QnStoppableAsync::pleaseStop
    */
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) = 0;

    /** Stops object's asynchronous operations and waits for completion.
        Default implementation calls \a QnStoppableAsync::pleaseStop and waits for completion
    */
    virtual void pleaseStopSync();

    typedef std::unique_ptr< QnStoppableAsync > UniquePtr;

    /**
        Calls \fn pleaseStop for each of \param stoppables and remove them after
        the last terminate is complete.
    */
    // TODO: #mux refactor with variadic template
    static void pleaseStop(nx::utils::MoveOnlyFunc< void() > completionHandler,
                            UniquePtr stoppable1 )
    {
        std::vector< UniquePtr > stoppables;
        stoppables.push_back( std::move( stoppable1 ) );
        pleaseStopImpl( std::move( stoppables ), std::move( completionHandler) );
    }
    static void pleaseStop(nx::utils::MoveOnlyFunc< void() > completionHandler,
                            UniquePtr stoppable1, UniquePtr stoppable2 )
    {
        std::vector< UniquePtr > stoppables;
        stoppables.push_back( std::move( stoppable1 ) );
        stoppables.push_back( std::move( stoppable2 ) );
        pleaseStopImpl( std::move( stoppables ), std::move( completionHandler) );
    }

private:
    static void pleaseStopImpl(
        std::vector< UniquePtr > stoppables,
        nx::utils::MoveOnlyFunc< void() > completionHandler );

    /*
    template< typename ... Args >
    static void pleaseStopImpl( std::vector< UniquePtr > stoppables,
                                std::function< void() > completionHandler,
                                UniquePtr stoppable1, Args ... args )
    {
        stoppables.push_back( std::move( stoppable1 ) );
        pleaseStopImpl( std::move( stoppables ), std::move( completionHandler),
                        std::move( args ) );
    }
    */
};

#endif  //QNSTOPPABLE_H
