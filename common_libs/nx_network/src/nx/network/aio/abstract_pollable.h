/**********************************************************
* Apr 11, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/async_stoppable.h>


namespace nx {
namespace network {
namespace aio {

class AbstractAioThread;

/** Abstract base for a class living in aio thread.
    TODO #ak concept is still a draft
*/
class AbstractPollable
:
    public QnStoppableAsync
{
public:
    virtual ~AbstractPollable() {}

    virtual aio::AbstractAioThread* getAioThread() const = 0;
    /** Generally, binding to aio thread can be done just after 
            object creation before any usage.
        Some implementation may allow more (e.g., binding if no async 
            operations are scheduled at the moment)
        NOTE: Re-binding is allowed
    */
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) = 0;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) = 0;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) = 0;
};

}   //namespace aio
}   //namespace network
}   //namespace nx
