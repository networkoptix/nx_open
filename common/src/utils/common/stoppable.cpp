
#include "stoppable.h"

#include <nx/utils/std/future.h>


void QnStoppableAsync::pleaseStopSync()
{
    nx::utils::promise<void> promise;
    auto fut = promise.get_future();
    pleaseStop( [&](){ promise.set_value(); } );
    fut.wait();
}

void QnStoppableAsync::pleaseStopImpl(
    std::vector< UniquePtr > stoppables,
    nx::utils::MoveOnlyFunc< void() > completionHandler )
{
    NX_ASSERT( completionHandler, Q_FUNC_INFO,
                "There is no other way to understend if QnStoppableAsync if stopped, "
                "but this handler" );

    std::vector< QnStoppableAsync* > tmpStoppables;
    for( auto& ptr : stoppables )
        tmpStoppables.push_back( ptr.get() );

    nx::BarrierHandler barrier(
        [stoppables = std::move(stoppables),
            completionHandler = std::move(completionHandler)]() mutable
        {
            stoppables.clear();
            completionHandler();
        });

    for( const auto& ptr : tmpStoppables )
        ptr->pleaseStop( barrier.fork() );
}
