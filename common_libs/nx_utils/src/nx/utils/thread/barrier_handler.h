#ifndef NX_UTILS_BARRIER_HANDLER_H
#define NX_UTILS_BARRIER_HANDLER_H

#include "mutex.h"

namespace nx {

/** Forks handler into several handlers and call the last one when
 *  the last fork is called */
class NX_UTILS_API BarrierHandler
{
public:
    BarrierHandler( std::function< void() > handler );
    ~BarrierHandler();

    std::function< void() > fork();

private:
    struct NX_UTILS_API Impl
    {
        Impl( std::function< void() > handler_ );

        QnMutex mutex;
        size_t counter;
        std::function< void() > handler;
    };

    std::shared_ptr< Impl > m_impl;
};

} // namespace nx

#endif // NX_UTILS_BARRIER_HANDLER_H
