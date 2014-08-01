/**********************************************************
* 20 may 2014
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef QN_CONCURRENT_H
#define QN_CONCURRENT_H

#include <cassert>
#include <functional>
#include <vector>
#include <type_traits>
#include <utility>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>
#include <QtCore/QThreadPool>
#include <QtCore/QSharedPointer>


/*!
    Contains analogous functions for some of \a QtConcurrent namespace, but provides way to run concurrent computations in custom thread pool with custom priority
*/
namespace QnConcurrent
{
    //TODO #ak QnConcurrent::mapped must accept member function pointer without std::mem_fn
    //TODO #ak QnConcurrent::run must accept any number of arguments (requires msvc2013)

    namespace detail
    {
        template<typename Function>
        class QnRunnableTask : public QRunnable
        {
            Function m_function;
        public:
            QnRunnableTask( Function function ) : m_function( function ) { setAutoDelete(true); }
            virtual void run() override { m_function(); }
        };

        template<class T>
        class QnFutureImplBase
        {
        public:
            typedef T value_type;
            typedef std::vector<bool>::size_type size_type;

            QnFutureImplBase()
            :
                m_totalTasksToRun( 0 ),
                m_tasksCompleted( 0 ),
                m_startedTaskCount( 0 ),
                m_isCancelled( false )
            {
            }

            ~QnFutureImplBase()
            {
                if( m_cleanupFunc )
                    m_cleanupFunc();
            }

            void setTotalTasksToRun( size_type totalTasksToRun )
            {
                m_totalTasksToRun = totalTasksToRun;
                m_completionMarks.resize( m_totalTasksToRun );
            }
            
            void waitForFinished()
            {
                QMutexLocker lk( &m_mutex );
                while( (!m_isCancelled && (m_tasksCompleted < m_totalTasksToRun)) ||
                       (m_isCancelled && (m_startedTaskCount > 0)) )
                {
                    m_cond.wait( lk.mutex() );
                }
            }

            void cancel()
            {
                QMutexLocker lk( &m_mutex );
                m_isCancelled = true;
            }

            bool isCanceled() const
            {
                QMutexLocker lk( &m_mutex );
                return m_isCancelled;
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
                return m_completionMarks[index];
            }

            bool incStartedTaskCountIfAllowed()
            {
                QMutexLocker lk( &m_mutex );
                if( m_isCancelled )
                    return false;
                ++m_startedTaskCount;
                return true;
            }

            void setCleanupFunc( std::function<void()> cleanupFunc )
            {
                m_cleanupFunc = cleanupFunc;
            }

        protected:
            mutable QMutex m_mutex;

            void setCompletedAtNonSafe( size_type index )
            {
                m_completionMarks[index] = true;
                ++m_tasksCompleted;
                taskStoppedNonSafe();
                m_cond.wakeAll();
            }

            void taskStoppedNonSafe()
            {
                assert( m_startedTaskCount >= 1 );
                --m_startedTaskCount;
            }

        private:
            QWaitCondition m_cond;
            size_t m_totalTasksToRun;
            size_t m_tasksCompleted;
            std::vector<bool> m_completionMarks;
            size_type m_startedTaskCount;
            bool m_isCancelled;
            std::function<void()> m_cleanupFunc;
        };

        template<class T>
        class QnFutureImpl
        :
            public QnFutureImplBase<T>
        {
            typedef QnFutureImplBase<T> base_type;

        public:
            typedef typename base_type::size_type size_type;

            void setTotalTasksToRun( size_type totalTasksToRun )
            {
                base_type::setTotalTasksToRun( totalTasksToRun );
                m_results.resize( totalTasksToRun );
            }

            T& resultAt( size_type index )
            {
                QMutexLocker lk( &this->m_mutex );
                return m_results[index];
            }

            const T& resultAt( size_type index ) const
            {
                QMutexLocker lk( &this->m_mutex );
                return m_results[index];
            }

            template<class Function, class Data>
            void executeFunctionOnDataAtPos( size_type index, Function function, const Data& data )
            {
                const T& result = function( data );
                QMutexLocker lk( &this->m_mutex );
                m_results[index] = std::move(result);
                this->setCompletedAtNonSafe( index );
            }

            template<class Function>
            void executeFunctionOnDataAtPos( size_type index, Function function )
            {
                const T& result = function();
                QMutexLocker lk( &this->m_mutex );
                m_results[index] = std::move(result);
                this->setCompletedAtNonSafe( index );
            }

        private:
            std::vector<T> m_results;
        };

        template<>
        class QnFutureImpl<void>
        :
            public QnFutureImplBase<void>
        {
        public:
            template<class Function, class Data>
            void executeFunctionOnDataAtPos( size_type index, Function function, const Data& data )
            {
                function( data );
                QMutexLocker lk( &this->m_mutex );
                this->setCompletedAtNonSafe( index );
            }

            template<class Function>
            void executeFunctionOnDataAtPos( size_type index, Function function )
            {
                function();
                QMutexLocker lk( &this->m_mutex );
                this->setCompletedAtNonSafe( index );
            }
        };

        template<typename Container>
        class safe_forward_iterator
        {
        public:
            typedef typename Container::size_type size_type;

            safe_forward_iterator( Container& container, typename Container::iterator&& startIter )
            :
                m_container( container ),
                m_currentIter( startIter ),
                m_currentIndex( std::distance( container.begin(), startIter ) )
            {
            }

            std::pair<typename Container::iterator, int> fetchAndMoveToNextPos()
            {
                QMutexLocker lk( &m_mutex );

                std::pair<typename Container::iterator, int> curVal( m_currentIter, m_currentIndex );
                if( m_currentIter != m_container.end() )
                {
                    ++m_currentIter;
                    ++m_currentIndex;
                }
                return curVal;
            }

            void moveToEnd()
            {
                QMutexLocker lk( &m_mutex );
                m_currentIter = m_container.end();
                m_currentIndex = m_container.size();
            }

        private:
            QMutex m_mutex;
            Container& m_container;
            typename Container::iterator m_currentIter;
            size_type m_currentIndex;
        };

        //!Executes task on element of a dictionary (used by \a QnConcurrent::mapped)
        template<typename Container, typename Function>
        class TaskExecuter
        {
        public:
            typedef typename std::result_of<Function(typename Container::value_type)>::type ResultType;
            typedef QnFutureImpl<typename std::result_of<Function(typename Container::value_type)>::type> FutureImplType;

            TaskExecuter(
                QThreadPool& threadPool,
                int priority,
                Container& container,
                Function function,
                QSharedPointer<FutureImplType> futureImpl,
                QSharedPointer<detail::safe_forward_iterator<Container> > safeIter )
            :
                m_threadPool( threadPool ),
                m_priority( priority ),
                m_container( container ),
                m_function( function ),
                m_futureImpl( futureImpl.toWeakRef() ),
                m_safeIter( safeIter )
            {
            }

            void operator()( const std::pair<typename Container::iterator, int>& val )
            {
                auto futureImplStrongRef = m_futureImpl.toStrongRef();
                assert( futureImplStrongRef );

                //const auto& result = m_function( *val.first );
                //launching next task
                const typename std::pair<typename Container::iterator, int>& nextElement = m_safeIter->fetchAndMoveToNextPos();
                if( nextElement.first != m_container.end() && futureImplStrongRef->incStartedTaskCountIfAllowed() )
                {
                    auto functor = std::bind( &detail::TaskExecuter<Container, Function>::operator(), this, nextElement );
                    m_threadPool.start(
                        new detail::QnRunnableTask<decltype(functor)>( std::move(functor) ),
                        m_priority );
                }
                //futureImplStrongRef->setResultAt( val.second, std::move(result) );
                futureImplStrongRef->executeFunctionOnDataAtPos( val.second, m_function, *val.first );
            }

        private:
            QThreadPool& m_threadPool;
            int m_priority;
            Container& m_container;
            Function m_function;
            QWeakPointer<FutureImplType> m_futureImpl;
            QSharedPointer<detail::safe_forward_iterator<Container> > m_safeIter;
        };
    }

    template<class T>
    class QnFutureBase
    {
    public:
        typedef typename detail::QnFutureImpl<T> FutureImplType;
        typedef typename FutureImplType::value_type value_type;
        typedef typename FutureImplType::size_type size_type;

        QnFutureBase() : m_impl( new FutureImplType() ) {}

        void waitForFinished() { m_impl->waitForFinished(); }
        void cancel() { m_impl->cancel(); }
        bool isCanceled() const { return m_impl->isCanceled(); }
        size_type progressValue() const { return m_impl->progressValue(); }
        size_type progressMinimum() const { return m_impl->progressMinimum(); }
        size_type progressMaximum() const { return m_impl->progressMaximum(); }
        size_type resultCount() const { return m_impl->resultCount(); }
        bool isResultReadyAt( size_type index ) const { return m_impl->isResultReadyAt( index ); }

        QSharedPointer<FutureImplType> impl() { return m_impl; }

    protected:
        QSharedPointer<FutureImplType> m_impl;
    };

    //!Result of concurrent task execution
    /*!
        Provides methods to wait for concurrent computation completion, get computation progress and get results

        \note Contains array of \a T elements
    */
    template<class T>
    class QnFuture
    :
        public QnFutureBase<T>
    {
    public:
        typedef typename QnFutureBase<T>::size_type size_type;

        T& resultAt( size_type index ) { return this->m_impl->resultAt( index ); }
        const T& resultAt( size_type index ) const { return this->m_impl->resultAt( index ); }
    };

    template<>
    class QnFuture<void>
    :
        public QnFutureBase<void>
    {
    public:
    };

    /*!
        \param threadPool
        \param priority Priority of execution in \a threadPool. 0 is a default priority
        \param container
        \param function To pass member-function here, you have to use std::mem_fn (for now)
    */
    template<typename Container, typename Function>
    QnFuture<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
        QThreadPool* threadPool,
        int priority,
        Container& container,
        Function function )
    {
        typedef QnFuture<typename std::result_of<Function(typename Container::value_type)>::type> FutureType;

        FutureType future;
        auto futureImpl = future.impl();
        futureImpl->setTotalTasksToRun( container.size() );
        QSharedPointer<detail::safe_forward_iterator<Container> > safeIter( new detail::safe_forward_iterator<Container>( container, container.begin() ) );

        typename detail::TaskExecuter<Container, Function>* taskExecutor =
            new detail::TaskExecuter<Container, Function>(
                *threadPool, priority, container, function, futureImpl, safeIter );
        futureImpl->setCleanupFunc( std::function<void()>( [taskExecutor](){ delete taskExecutor; } ) );    //TODO #ak not good! Think over again

        //launching maximum threadPool->maxThreadCount() tasks, other tasks added to threadPool's queue after completion of added tasks
        const int maxTasksToLaunch = threadPool->maxThreadCount();
        for( int tasksLaunched = 0;
            tasksLaunched < maxTasksToLaunch;
            ++tasksLaunched )
        {
            const typename std::pair<typename Container::iterator, int>& nextElement = safeIter->fetchAndMoveToNextPos();
            if( nextElement.first == container.end() )
                break;  //all tasks processed

            if( !futureImpl->incStartedTaskCountIfAllowed() )
            {
                Q_ASSERT( false );
            }
            auto functor = std::bind( &detail::TaskExecuter<Container, Function>::operator(), taskExecutor, nextElement );
            threadPool->start(
                new detail::QnRunnableTask<decltype(functor)>( std::move(functor) ),
                priority );
        }
        return future;
    }

    /*!
        Executes with default priority
        \param function To pass member-function here, you have to use std::mem_fn (for now)
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
        \param function To pass member-function here, you have to use std::mem_fn (for now)
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
            futureImpl->executeFunctionOnDataAtPos( 0, function );
        };
        if( !futureImpl->incStartedTaskCountIfAllowed() )
        {
            Q_ASSERT( false );
        }
        threadPool->start(
            new detail::QnRunnableTask<decltype(taskRunFunction)>( taskRunFunction ),
            priority );
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
