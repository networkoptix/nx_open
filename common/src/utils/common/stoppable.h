/**********************************************************
* 28 jan 2013
* a.kolesnikov
***********************************************************/

#ifndef QNSTOPPABLE_H
#define QNSTOPPABLE_H

#include <functional>
#include <vector>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/barrier_handler.h>


/** Abstract class providing interface to stop doing anything without object destruction */
class QN_EXPORT QnStoppable
{
public:
    virtual ~QnStoppable() = default;

    virtual void pleaseStop() = 0;
};

/** Abstract interface to interrupt asynchronous operation with completion notification */
class QN_EXPORT QnStoppableAsync
{
public:
    virtual ~QnStoppableAsync() = default;

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
    virtual void pleaseStopSync(bool checkForLocks = true);
};

#endif  //QNSTOPPABLE_H
