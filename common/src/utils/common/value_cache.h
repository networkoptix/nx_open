#ifndef VALUE_CACHE_H
#define VALUE_CACHE_H

#include <functional>

#include <boost/optional.hpp>

#include <QMutex>
#include <QMutexLocker>


/*!
    \a get() and \a update() may be called safely fom different threads
*/
template<class ValueType>
class CachedValue
{
public:
    /*!
        \param valGenerationFunc This functor is called from get() and update() with no synchronization.
            So it should be synchronized by itself (if it is needed)
        \note \a valGenerationFunc is not called here!
    */
    template<class FuncType>
    CachedValue(
        FuncType&& valGenerationFunc,
        QnMutex* const mutex )
    :
        m_valGenerationFunc( std::forward<FuncType>(valGenerationFunc) ),
        m_mutex( mutex )
    {
    }

    ValueType get() const
    {
        SCOPED_MUTEX_LOCK( lk, m_mutex );
        if( !m_cachedVal )
        {
            lk.unlock();
            const ValueType newVal = m_valGenerationFunc();
            lk.relock();
            if( !m_cachedVal )
                m_cachedVal = newVal;
        }

        return m_cachedVal.get();
    }

    void reset()
    {
        QMutexLocker lk( m_mutex );
        m_cachedVal.reset();
    }

    void update()
    {
        const ValueType newVal = m_valGenerationFunc();
        SCOPED_MUTEX_LOCK( lk, m_mutex );
        m_cachedVal = newVal;
    }

private:
    mutable boost::optional<ValueType> m_cachedVal;
    std::function<ValueType()> m_valGenerationFunc;
    QnMutex* const m_mutex;
};

#endif  //VALUE_CACHE_H
