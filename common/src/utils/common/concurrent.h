/**********************************************************
* 20 may 2014
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef QN_CONCURRENT_H
#define QN_CONCURRENT_H

#include <vector>
#include <utility>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>
#include <QtCore/QThreadPool>


/*!
    Contains analogous functions for some of \a QtConcurrent namespace, but provides way to run concurrent computations in custom thread pool with custom priority
*/
namespace QnConcurrent
{
    //TODO #ak support QnFuture<void>
    //TODO #ak QnConcurrent::mapped must accept member function pointer without std::mem_fn
    //TODO #ak QnConcurrent::run must accept any number of arguments

    namespace detail
    {
        template<typename Function>
        class QnRunnableTask : public QRunnable
        {
            Function m_function;
        public:
            QnRunnableTask( Function function ) : m_function( function ) {}
            virtual void run() override { m_function(); }
        };

        template<class T>
        class QnFutureImpl
        {
        public:
            typedef T value_type;
            typedef typename std::vector<typename std::pair<T, bool> >::size_type size_type;

            QnFutureImpl()
            :
                m_totalTasksToRun( 0 ),
                m_tasksCompleted( 0 )
            {
            }

            void setTotalTasksToRun( size_type totalTasksToRun )
            {
                m_totalTasksToRun = totalTasksToRun;
                m_results.resize( m_totalTasksToRun );
            }
            
            void waitForFinished()
            {
                QMutexLocker lk( &m_mutex );
                while( m_tasksCompleted < m_totalTasksToRun )
                    m_cond.wait( lk.mutex() );
            }

            void cancel()
            {
                //TODO #ak
            }

            bool isCanceled() const
            {
                //TOOD #ak
                return false;
            }

            size_type progressValue() const
            {
                QMutexLocker lk( &m_mutex );
                return m_tasksCompleted;
            }

            size_type progressMinimum() const
            {
                QMutexLocker lk( &m_mutex );
                return 0;
            }

            size_type progressMaximum() const
            {
                QMutexLocker lk( &m_mutex );
                return m_totalTasksToRun;
            }

            size_type resultCount() const
            {
                QMutexLocker lk( &m_mutex );
                return m_tasksCompleted;
            }

            bool isResultReadyAt( size_type index ) const
            {
                QMutexLocker lk( &m_mutex );
                return m_results[index].second;
            }

            T& resultAt( size_type index )
            {
                QMutexLocker lk( &m_mutex );
                return m_results[index].first;
            }

            const T& resultAt( size_type index ) const
            {
                QMutexLocker lk( &m_mutex );
                return m_results[index].first;
            }

            void setResultAt( size_type index, T&& result )
            {
                QMutexLocker lk( &m_mutex );
                m_results[index].first = result;
                m_results[index].second = true;
                ++m_tasksCompleted;
                m_cond.wakeAll();
            }

            void setResultAt( size_type index, const T& result )
            {
                QMutexLocker lk( &m_mutex );
                m_results[index].first = result;
                m_results[index].second = true;
                ++m_tasksCompleted;
                m_cond.wakeAll();
            }

        private:
            mutable QMutex m_mutex;
            QWaitCondition m_cond;
            size_t m_totalTasksToRun;
            size_t m_tasksCompleted;
            std::vector<std::pair<T, bool> > m_results;
        };
    }

    //!Result of concurrent task execution
    /*!
        Provides methods to wait for concurrent computation completion, get computation progress and get results

        \note Contains array of \a T elements
    */
    template<class T>
    class QnFuture
    {
    public:
        typedef typename detail::QnFutureImpl<T>::value_type value_type;
        typedef typename detail::QnFutureImpl<T>::size_type size_type;

        QnFuture() : m_impl( new detail::QnFutureImpl<T>() ) {}

        void waitForFinished() { m_impl->waitForFinished(); }
        void cancel() { m_impl->cancel(); }
        bool isCanceled() const { return m_impl->isCanceled(); }
        size_type progressValue() const { return m_impl->progressValue(); }
        size_type progressMinimum() const { return m_impl->progressMinimum(); }
        size_type progressMaximum() const { return m_impl->progressMaximum(); }
        size_type resultCount() const { return m_impl->resultCount(); }
        bool isResultReadyAt( size_type index ) const { return m_impl->isResultReadyAt( index ); }
        T& resultAt( size_type index ) { return m_impl->resultAt( index ); }
        const T& resultAt( size_type index ) const { return m_impl->resultAt( index ); }

        QSharedPointer<detail::QnFutureImpl<T> > impl() { return m_impl; }

    private:
        QSharedPointer<detail::QnFutureImpl<T> > m_impl;
    };

    /*!
        \param priority Priority of execution in \a threadPool. 0 is a default priority
        \param function To pass member-function here, you have to use std::mem_fn
    */
    template<typename Container, typename Function>
    QnFuture<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
        QThreadPool* threadPool,
        int priority,
        Container& container,
        Function function )
    {
        QnFuture<typename std::result_of<Function(typename Container::value_type)>::type> future;
        auto futureImpl = future.impl();
        futureImpl->setTotalTasksToRun( container.size() );
        size_t index = 0;
        //TODO #ak should enqueue tasks as they complete so that to allow other concurrent functions to run
        for( Container::iterator
            it = container.begin();
            it != container.end();
            ++it )
        {
            auto& val = *it;
            auto taskRunFunction = [&val, function, futureImpl, index](){
                futureImpl->setResultAt( index, function( val ) );
            };
            auto task = new detail::QnRunnableTask<decltype(taskRunFunction)>( taskRunFunction );
            task->setAutoDelete( true );
            threadPool->start( task, priority );
            ++index;
        }
        return future;
    }

    /*!
        Executes with default priority
        \param function To pass member-function here, you have to use std::mem_fn
    */
    template<typename Container, typename Function>
    QnFuture<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
        QThreadPool* threadPool,
        Container& container,
        Function function )
    {
        return mapped( threadPool, 0, container, function );
    }

    /*!
        Executes in global thread pool with default priority
        \param function To pass member-function here, you have to use std::mem_fn
    */
    template<typename Container, typename Function>
    QnFuture<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
        Container& container,
        Function function )
    {
        return mapped( *QThreadPool::globalInstance(), 0, container, function );
    }

    /*!
        Runs \a function in \a threadPool with \a priority

        \note Execution cannot be canceled
    */
    template<typename Function>
    QnFuture<typename std::result_of<Function()>::type> run( QThreadPool* threadPool, int priority, Function function )
    {
        QnFuture<typename std::result_of<Function()>::type> future;
        auto futureImpl = future.impl();
        futureImpl->setTotalTasksToRun( 1 );
        auto taskRunFunction = [function, futureImpl]() {
            futureImpl->setResultAt( 0, function() );
        };
        auto task = new detail::QnRunnableTask<decltype(taskRunFunction)>( taskRunFunction );
        task->setAutoDelete( true );
        threadPool->start( task, priority );
        return future;
    }

    /*!
        Runs \a function in \a threadPool with default priority
    */
    template<typename Function>
    QnFuture<typename std::result_of<Function()>::type> run( QThreadPool* threadPool, Function function )
    {
        return run( threadPool, 0, function );
    }

    /*!
        Runs \a function in global thread pool with default priority
    */
    template<typename Function>
    QnFuture<typename std::result_of<Function()>::type> run( Function function )
    {
        return run( *QThreadPool::globalInstance(), 0, function );
    }
}

#endif  //QN_CONCURRENT_H
