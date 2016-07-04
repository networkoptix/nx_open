/**********************************************************
* Jun 30, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "abstract_pollable.h"

#include "timer.h"


namespace nx {
namespace network {
namespace aio {

/** TODO #ak conflicts with Pollable. Introduce proper names for Pollable, AbstractPollable, BasicPollable */
class NX_NETWORK_API BasicPollable
:
    public AbstractPollable
{
public:
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler);
    virtual void pleaseStopSync();

    virtual aio::AbstractAioThread* getAioThread() const override;
    /** Generally, binding to aio thread can be done just after 
            object creation before any usage.
        Some implementation may allow more (e.g., binding if no async 
            operations are scheduled at the moment)
        \note Re-binding is allowed
    */
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    virtual void stopWhileInAioThread() = 0;

private:
    Timer m_timer;
};

}   //namespace aio
}   //namespace network
}   //namespace nx
