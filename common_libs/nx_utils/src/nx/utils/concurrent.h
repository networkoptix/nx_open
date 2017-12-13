#pragma once

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <QtCore/QThreadPool>
#include <QtCore/QSharedPointer>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace utils {

/**
 * Contains analogous functions for some of QtConcurrent namespace,
 * but provides a way to run concurrent computations in custom thread pool with custom priority.
 */
namespace concurrent {

// TODO: #ak nx::utils::concurrent::mapped must accept member function pointer without std::mem_fn.
// TODO: #ak nx::utils::concurrent::run must accept any number of arguments (requires msvc2013).

namespace detail {

template<typename Function>
class RunnableTask: public QRunnable
{
public:
    RunnableTask(Function function): m_function(function) { setAutoDelete(true); }
    virtual void run() override { m_function(); }

private:
    Function m_function;
};

template<class T>
class FutureImplBase
{
public:
    typedef T value_type;
    typedef std::vector<bool>::size_type size_type;

    FutureImplBase(size_type totalTasksToWaitFor = 0):
        m_totalTasksToRun(totalTasksToWaitFor),
        m_tasksCompleted(0),
        m_startedTaskCount(totalTasksToWaitFor),
        m_isCancelled(false)
    {
        m_completionMarks.resize(m_totalTasksToRun);
    }

    ~FutureImplBase()
    {
        if (m_cleanupFunc)
            m_cleanupFunc();
    }

    void setTotalTasksToRun(size_type totalTasksToRun)
    {
        m_totalTasksToRun = totalTasksToRun;
        m_completionMarks.resize(m_totalTasksToRun);
    }

    void waitForFinished()
    {
        QnMutexLocker lock(&m_mutex);
        while ((!m_isCancelled && m_tasksCompleted < m_totalTasksToRun) ||
            (m_isCancelled && m_startedTaskCount > 0))
        {
            m_cond.wait(lock.mutex());
        }
    }

    void cancel()
    {
        QnMutexLocker lock(&m_mutex);
        m_isCancelled = true;
    }

    bool isInProgress() const
    {
        QnMutexLocker lock(&m_mutex);
        return (!m_isCancelled && (m_tasksCompleted < m_totalTasksToRun)) ||
            (m_isCancelled && (m_startedTaskCount > 0));
    }

    bool isCanceled() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_isCancelled;
    }

    size_type progressValue() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_tasksCompleted;
    }

    size_type progressMinimum() const
    {
        QnMutexLocker lock(&m_mutex);
        return 0;
    }

    size_type progressMaximum() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_totalTasksToRun;
    }

    size_type resultCount() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_tasksCompleted;
    }

    bool isResultReadyAt(size_type index) const
    {
        QnMutexLocker lock(&m_mutex);
        return m_completionMarks[index];
    }

    bool incStartedTaskCountIfAllowed()
    {
        QnMutexLocker lock(&m_mutex);
        if (m_isCancelled)
            return false;
        ++m_startedTaskCount;
        return true;
    }

    void setCleanupFunc(std::function<void()> cleanupFunc)
    {
        m_cleanupFunc = cleanupFunc;
    }

protected:
    mutable QnMutex m_mutex;

    void setCompletedAtNonSafe(size_type index)
    {
        m_completionMarks[index] = true;
        ++m_tasksCompleted;
        taskStoppedNonSafe();
        m_cond.wakeAll();
    }

    void taskStoppedNonSafe()
    {
        NX_ASSERT(m_startedTaskCount >= 1);
        --m_startedTaskCount;
    }

private:
    QnWaitCondition m_cond;
    size_t m_totalTasksToRun;
    size_t m_tasksCompleted;
    std::vector<bool> m_completionMarks;
    size_type m_startedTaskCount;
    bool m_isCancelled;
    std::function<void()> m_cleanupFunc;
};

template<class T>
class QnFutureImpl:
    public FutureImplBase<T>
{
    typedef FutureImplBase<T> base_type;

public:
    typedef typename base_type::size_type size_type;
    typedef typename std::vector<T>::reference reference;
    typedef typename std::vector<T>::const_reference const_reference;

    QnFutureImpl(size_type totalTasksToWaitFor = 0):
        base_type(totalTasksToWaitFor)
    {
        m_results.resize(totalTasksToWaitFor);
    }

    void setTotalTasksToRun(size_type totalTasksToRun)
    {
        base_type::setTotalTasksToRun(totalTasksToRun);
        m_results.resize(totalTasksToRun);
    }

    reference resultAt(size_type index)
    {
        QnMutexLocker lock(&this->m_mutex);
        return m_results[index];
    }

    const_reference resultAt(size_type index) const
    {
        QnMutexLocker lock(&this->m_mutex);
        return m_results[index];
    }

    template<class Function, class Data>
    void executeFunctionOnDataAtPos(size_type index, Function function, const Data& data)
    {
        setResultAt(index, function(data));
    }

    template<class Function>
    void executeFunctionOnDataAtPos(size_type index, Function function)
    {
        setResultAt(index, function());
    }

    template<class ResultType>
    void setResultAt(size_type index, ResultType&& result)
    {
        QnMutexLocker lock(&this->m_mutex);
        m_results[index] = std::forward<ResultType>(result);
        this->setCompletedAtNonSafe(index);
    }

private:
    std::vector<T> m_results;
};

template<>
class QnFutureImpl<void>:
    public FutureImplBase<void>
{
    typedef FutureImplBase<void> base_type;

public:
    QnFutureImpl(size_type totalTasksToWaitFor = 0):
        base_type(totalTasksToWaitFor)
    {
    }

    template<class Function, class Data>
    void executeFunctionOnDataAtPos(size_type index, Function function, const Data& data)
    {
        function(data);
        setResultAt(index);
    }

    template<class Function>
    void executeFunctionOnDataAtPos(size_type index, Function function)
    {
        function();
        setResultAt(index);
    }

    void setResultAt(size_type index)
    {
        QnMutexLocker lock(&this->m_mutex);
        this->setCompletedAtNonSafe(index);
    }
};

template<typename Container>
class safe_forward_iterator
{
public:
    typedef typename Container::size_type size_type;

    safe_forward_iterator(Container& container, typename Container::iterator&& startIter):
        m_container(container),
        m_currentIter(startIter),
        m_currentIndex(std::distance(container.begin(), startIter))
    {
    }

    std::pair<typename Container::iterator, int> fetchAndMoveToNextPos()
    {
        QnMutexLocker lock(&m_mutex);

        //TODO #ak is int appropriate here?
        std::pair<typename Container::iterator, int> curVal(m_currentIter, (int) m_currentIndex);
        if (m_currentIter != m_container.end())
        {
            ++m_currentIter;
            ++m_currentIndex;
        }
        return curVal;
    }

    void moveToEnd()
    {
        QnMutexLocker lock(&m_mutex);
        m_currentIter = m_container.end();
        m_currentIndex = m_container.size();
    }

private:
    QnMutex m_mutex;
    Container& m_container;
    typename Container::iterator m_currentIter;
    size_type m_currentIndex;
};

/**
 * Executes task on element of a dictionary (used by nx::utils::concurrent::mapped).
 */
template<typename Container, typename Function>
class TaskExecuter
{
public:
    using ResultType = typename std::result_of<Function(typename Container::value_type)>::type;
    using FutureImplType =
        QnFutureImpl<typename std::result_of<Function(typename Container::value_type)>::type>;

    TaskExecuter(
        QThreadPool& threadPool,
        int priority,
        Container& container,
        Function function,
        QSharedPointer<FutureImplType> futureImpl,
        QSharedPointer<detail::safe_forward_iterator<Container>> safeIter)
        :
        m_threadPool(threadPool),
        m_priority(priority),
        m_container(container),
        m_function(function),
        m_futureImpl(futureImpl.toWeakRef()),
        m_safeIter(safeIter)
    {
    }

    void operator()(const std::pair<typename Container::iterator, int>& val)
    {
        auto futureImplStrongRef = m_futureImpl.toStrongRef();
        NX_ASSERT(futureImplStrongRef);

        // Launching next task.
        const typename std::pair<typename Container::iterator, int>& nextElement =
            m_safeIter->fetchAndMoveToNextPos();
        if (nextElement.first != m_container.end() &&
            futureImplStrongRef->incStartedTaskCountIfAllowed())
        {
            auto functor = std::bind(
                &detail::TaskExecuter<Container, Function>::operator(), this, nextElement);
            m_threadPool.start(
                new detail::RunnableTask<decltype(functor)>(std::move(functor)),
                m_priority);
        }
        futureImplStrongRef->executeFunctionOnDataAtPos(val.second, m_function, *val.first);
    }

private:
    QThreadPool& m_threadPool;
    int m_priority;
    Container& m_container;
    Function m_function;
    QWeakPointer<FutureImplType> m_futureImpl;
    QSharedPointer<detail::safe_forward_iterator<Container>> m_safeIter;
};

} // namespace detail

template<class T>
class QnFutureBase
{
public:
    typedef typename detail::QnFutureImpl<T> FutureImplType;
    typedef typename FutureImplType::value_type value_type;
    typedef typename FutureImplType::size_type size_type;

    QnFutureBase(size_type totalTasksToWaitFor = 0):
        m_impl(new FutureImplType(totalTasksToWaitFor))
    {
    }

    void waitForFinished() { m_impl->waitForFinished(); }
    void cancel() { m_impl->cancel(); }
    bool isInProgress() const { return m_impl->isInProgress(); }
    bool isCanceled() const { return m_impl->isCanceled(); }
    size_type progressValue() const { return m_impl->progressValue(); }
    size_type progressMinimum() const { return m_impl->progressMinimum(); }
    size_type progressMaximum() const { return m_impl->progressMaximum(); }
    size_type resultCount() const { return m_impl->resultCount(); }
    bool isResultReadyAt(size_type index) const { return m_impl->isResultReadyAt(index); }

    QSharedPointer<FutureImplType> impl() { return m_impl; }

protected:
    QSharedPointer<FutureImplType> m_impl;
};

/**
 * Result of concurrent task execution.
 * Provides methods to wait for concurrent computation completion, get computation progress and get results.
 *
 * NOTE: Contains array of T elements.
 */
template<class T>
class Future:
    public QnFutureBase<T>
{
    typedef QnFutureBase<T> base_type;

public:
    typedef typename base_type::size_type size_type;
    typedef typename base_type::FutureImplType::reference reference;
    typedef typename base_type::FutureImplType::const_reference const_reference;

    Future(size_type totalTasksToWaitFor = 0):
        base_type(totalTasksToWaitFor)
    {
    }

    reference resultAt(size_type index)
    {
        return this->m_impl->resultAt(index);
    }

    const_reference resultAt(size_type index) const
    {
        return this->m_impl->resultAt(index);
    }

    template<class ResultType>
    void setResultAt(size_type index, ResultType&& result)
    {
        this->m_impl->setResultAt(index, std::forward<ResultType>(result));
    }
};

template<>
class Future<void>:
    public QnFutureBase<void>
{
    typedef QnFutureBase<void> base_type;

public:
    Future(size_type totalTasksToWaitFor = 0):
        base_type(totalTasksToWaitFor)
    {
    }

    void setResultAt(size_type index)
    {
        this->m_impl->setResultAt(index);
    }
};

static constexpr int kDefaultTaskPriority = 0;

/**
 * @param priority Priority of execution in threadPool. 0 is a default priority
 * @param function To pass member-function here, you have to use std::mem_fn (for now)
 */
template<typename Container, typename Function>
Future<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
    QThreadPool* threadPool,
    int priority,
    Container& container,
    Function function)
{
    using FutureType =
        Future<typename std::result_of<Function(typename Container::value_type)>::type>;

    FutureType future;
    auto futureImpl = future.impl();
    futureImpl->setTotalTasksToRun(container.size());
    QSharedPointer<detail::safe_forward_iterator<Container>> safeIter(
        new detail::safe_forward_iterator<Container>(container, container.begin()));

    typename detail::TaskExecuter<Container, Function>* taskExecutor =
        new detail::TaskExecuter<Container, Function>(
            *threadPool, priority, container, function, futureImpl, safeIter);
    futureImpl->setCleanupFunc(
        std::function<void()>([taskExecutor]() { delete taskExecutor; }));    //TODO #ak not good! Think over again

    // Launching maximum threadPool->maxThreadCount() tasks,
    // other tasks added to threadPool's queue after completion of added tasks.
    const int maxTasksToLaunch = threadPool->maxThreadCount();
    for (int tasksLaunched = 0;
        tasksLaunched < maxTasksToLaunch;
        ++tasksLaunched)
    {
        const typename std::pair<typename Container::iterator, int>&
            nextElement = safeIter->fetchAndMoveToNextPos();
        if (nextElement.first == container.end())
            break;  //all tasks processed

        if (!futureImpl->incStartedTaskCountIfAllowed())
        {
            NX_ASSERT(false);
        }
        auto functor = std::bind(&detail::TaskExecuter<Container, Function>::operator(), taskExecutor, nextElement);
        threadPool->start(
            new detail::RunnableTask<decltype(functor)>(std::move(functor)),
            priority);
    }
    return future;
}

/**
 * Executes function with default priority.
 * @param function To pass member-function here, you have to use std::mem_fn (for now).
 */
template<typename Container, typename Function>
Future<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
    QThreadPool* threadPool,
    Container& container,
    Function function)
{
    return mapped(threadPool, kDefaultTaskPriority, container, function);
}

/**
 * Executes function in global thread pool with default priority.
 * @param function To pass member-function here, you have to use std::mem_fn (for now).
 */
template<typename Container, typename Function>
Future<typename std::result_of<Function(typename Container::value_type)>::type> mapped(
    Container& container,
    Function function)
{
    return mapped(QThreadPool::globalInstance(), kDefaultTaskPriority, container, function);
}

/**
 * Runs function in threadPool with priority.
 * NOTE: Execution cannot be canceled.
 */
template<typename Function>
Future<typename std::result_of<Function()>::type> run(
    QThreadPool* threadPool,
    int priority,
    Function function)
{
    Future<typename std::result_of<Function()>::type> future;
    auto futureImpl = future.impl();
    futureImpl->setTotalTasksToRun(1);
    auto taskRunFunction =
        [function, futureImpl]()
        {
            futureImpl->executeFunctionOnDataAtPos(0, function);
        };
    if (!futureImpl->incStartedTaskCountIfAllowed())
    {
        NX_ASSERT(false);
    }
    threadPool->start(
        new detail::RunnableTask<decltype(taskRunFunction)>(taskRunFunction),
        priority);
    return future;
}

/**
 * Runs function in threadPool with default priority.
 */
template<typename Function>
Future<typename std::result_of<Function()>::type> run(
    QThreadPool* threadPool,
    Function function)
{
    return run(threadPool, kDefaultTaskPriority, function);
}

/**
 * Runs function in global thread pool with default priority.
 */
template<typename Function>
Future<typename std::result_of<Function()>::type> run(Function function)
{
    return run(QThreadPool::globalInstance(), kDefaultTaskPriority, function);
}

} // namespace concurrent
} // namespace utils
} // namespace nx
