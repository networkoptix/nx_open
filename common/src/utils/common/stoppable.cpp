#include "stoppable.h"

void QnStoppableAsync::pleaseStopSync()
{
    std::promise<void> promise;
    auto fut = promise.get_future();
    pleaseStop( [&](){ promise.set_value(); } );
    fut.wait();
}

void QnStoppableAsync::pleaseStopImpl( std::vector< UniquePtr > stoppables,
                                       std::function< void() > completionHandler )
{
    std::vector< QnStoppableAsync* > tmpStoppables;
    for( auto& ptr : stoppables )
        tmpStoppables.push_back( ptr.get() );

    // TODO: #mux refactor with move lambda
    auto sharedStopables = std::make_shared<
            std::vector< std::unique_ptr< QnStoppableAsync > > >(
                std::move( stoppables ) );
    auto sharedHandler = std::make_shared<
            std::function< void() > >( std::move( completionHandler ) );

    nx::BarrierHandler barrier( [ = ]()
    {
        sharedStopables->clear();
        sharedHandler->operator()();
    } );

    for( const auto& ptr : tmpStoppables )
        ptr->pleaseStop( barrier.fork() );
}
