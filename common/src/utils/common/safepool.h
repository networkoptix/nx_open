/**********************************************************
* 15 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef SAFEPOOL_H
#define SAFEPOOL_H

#include <map>

#include <utils/common/mutex.h>
#include <utils/common/mutex.h>
#include <utils/common/wait_condition.h>


//!Associative thread-safe container, which allows to lock elements from accessing from other threads
/*!
    \note All methods are thread-safe
    \note All operations on locked element MUST occur from locking thread, otherwise undefined behavour can occur
*/
template<class KeyType, class MappedType>
class SafePool
{
    typedef typename std::map<KeyType, MappedType> InternalContainerType;

public:
    typedef typename InternalContainerType::value_type value_type;
    typedef typename InternalContainerType::size_type size_type;
    typedef typename InternalContainerType::const_reference const_reference;
    typedef typename InternalContainerType::reference reference;
    typedef typename InternalContainerType::const_pointer const_pointer;
    typedef typename InternalContainerType::pointer pointer;

    class const_iterator
    {
        friend class iterator;
        friend class SafePool;

    public:
        const_iterator()
        {
        }

        const_reference operator*() const
        {
            return *m_iter;
        }

        const_pointer operator->() const
        {
            return m_iter.operator->();
        }

        bool operator==( const const_iterator& right ) const
        {
            return m_iter == right.m_iter;
        }

        bool operator!=( const const_iterator& right ) const
        {
            return m_iter != right.m_iter;
        }

    private:
        typename InternalContainerType::const_iterator m_iter;

        const_iterator( const typename InternalContainerType::const_iterator& iter )
        :
            m_iter( iter )
        {
        }
    };

    class iterator
    {
        friend class SafePool;

    public:
        reference operator*() const
        {
            return *m_iter;
        }

        pointer operator->() const
        {
            return m_iter.operator->();
        }

        operator const_iterator() const
        {
            return const_iterator( m_iter );
        }

        bool operator==( const iterator& right ) const
        {
            return m_iter == right.m_iter;
        }

        bool operator!=( const iterator& right ) const
        {
            return m_iter != right.m_iter;
        }

    private:
        typename InternalContainerType::iterator m_iter;

        iterator( const typename InternalContainerType::iterator& iter )
        :
            m_iter( iter )
        {
        }
    };

    //!Locks element with key \a key
    /*!
        If requested element is already locked, blocks till element is unlocked
        \return end() if no element with \a key
        \note Recursive locking of element by single thread causes a dead-lock
    */
    const_iterator lock( const KeyType& key ) const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        typename InternalContainerType::const_iterator it = m_dataDict.end();
        typename SyncCtxDict::iterator syncCtxIter = m_syncCtxDict.end();
        for( ;; )
        {
            it = m_dataDict.find( key );
            if( it == m_dataDict.end() )
                return const_iterator( m_dataDict.end() );

            syncCtxIter = m_syncCtxDict.find( key );
            Q_ASSERT( syncCtxIter != m_syncCtxDict.end() );
            if( !syncCtxIter->second.locked )
                break;
            m_cond.wait( lk.mutex() );
            //element could be removed while we were waiting, so repeating search...
        }

        syncCtxIter->second.locked = true;
        return it;
    }

    //!Locks element with key \a key
    /*!
        \return end() if no element with \a key
    */
    iterator lock( const KeyType& key )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        typename InternalContainerType::iterator it = m_dataDict.end();
        typename SyncCtxDict::iterator syncCtxIter = m_syncCtxDict.end();
        for( ;; )
        {
            it = m_dataDict.find( key );
            if( it == m_dataDict.end() )
                return iterator( m_dataDict.end() );

            syncCtxIter = m_syncCtxDict.find( key );
            Q_ASSERT( syncCtxIter != m_syncCtxDict.end() );
            if( !syncCtxIter->second.locked )
                break;
            m_cond.wait( lk.mutex() );
            //element could be removed while we were waiting, so repeating search...
        }

        syncCtxIter->second.locked = true;
        return it;
    }

    /*!
        If no element with \a key, than \a *lockedIter is set to \a end() and false is returned
    */
    bool tryLock( const KeyType& key, const_iterator* const lockedIter ) const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        typename InternalContainerType::const_iterator it = m_dataDict.find( key );
        if( it == m_dataDict.end() )
        {
            *lockedIter = const_iterator( m_dataDict.end() );  //Cannot call end() here since it locks m_mutex
            return false;
        }

        typename SyncCtxDict::iterator syncCtxIter = m_syncCtxDict.find( key );
        Q_ASSERT( syncCtxIter != m_syncCtxDict.end() );
        if( syncCtxIter->second.locked )
            return false;

        syncCtxIter->second.locked = true;
        *lockedIter = it;
        return true;
    }

    void unlock( const const_iterator& it ) const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        typename SyncCtxDict::iterator syncCtxIter = m_syncCtxDict.find( it->first );
        Q_ASSERT( syncCtxIter != m_syncCtxDict.end() );
        Q_ASSERT( syncCtxIter->second.locked );

        syncCtxIter->second.locked = false;
        m_cond.wakeAll();
    }

    //!Atomically inserts new element
    /*!
        \return true, if inserted. false, if element with such key already exists and nothing has been done
    */
    std::pair<iterator, bool> insert( const value_type& val )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        const typename std::pair<typename InternalContainerType::iterator, bool>& p = m_dataDict.insert( val );
        if( !p.second )
            return std::make_pair( iterator(p.first), false );
        m_syncCtxDict.insert( std::make_pair(val.first, SynchronizationContext()) );

        return std::make_pair( iterator(p.first), true );
    }

    //!Inserts new element and atomically locks it
    /*!
        \return true, if inserted and locked. false, if element with such key already exists and nothing has been done
    */
    std::pair<iterator, bool> insertAndLock( const value_type& val )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        const typename std::pair<typename InternalContainerType::iterator, bool>& p = m_dataDict.insert( val );
        if( !p.second )
            return std::make_pair( iterator(p.first), false );
        m_syncCtxDict.insert( std::make_pair(val.first, SynchronizationContext(true)) );

        return std::make_pair( iterator(p.first), true );
    }

    //!Erases locked element
    /*!
        \note Element MUST be locked by calling thread, otherwise undefined behavour can occur
    */
    void eraseAndUnlock( const iterator& it )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        m_syncCtxDict.erase( it->first );
        m_dataDict.erase( it.m_iter );
        m_cond.wakeAll();
    }

    size_type size() const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return m_dataDict.size();
    }

    bool empty() const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return m_dataDict.empty();
    }

    const_iterator begin() const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return const_iterator( m_dataDict.begin() );
    }

    iterator begin()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return iterator( m_dataDict.begin() );
    }

    const_iterator end() const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return const_iterator( m_dataDict.end() );
    }

    iterator end()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return iterator( m_dataDict.end() );
    }

    void clear()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_dataDict.clear();
        m_syncCtxDict.clear();
        m_cond.wakeAll();
    }

    //!Atommically builds list of keys of all elements in dictionary
    /*!
        \note There is no garantee that any item will not be removed by any other thread in any moment
    */
    std::vector<KeyType> keys() const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        std::vector<KeyType> keysVector;
        for( typename InternalContainerType::const_iterator
            it = m_dataDict.begin();
            it != m_dataDict.end();
            ++it )
        {
            keysVector.push_back( it->first );
        }

        return keysVector;
    }

    class ScopedReadLock
    {
    public:
        ScopedReadLock( const SafePool& pool, const typename SafePool::const_iterator iter )
        :
            m_pool( pool ),
            m_iter( iter )
        {
        }

        ~ScopedReadLock()
        {
            if( isValid() )
                m_pool.unlock( m_iter );
        }

        bool isValid() const
        {
            return m_iter != m_pool.end();
        }

        const_reference operator*() const
        {
            return *m_iter;
        }

        const_pointer operator->() const
        {
            return m_iter.operator->();
        }

    private:
        const SafePool& m_pool;
        const typename SafePool::const_iterator m_iter;

        ScopedReadLock( const ScopedReadLock& );
        ScopedReadLock& operator=( const ScopedReadLock& );
    };

private:
    class SynchronizationContext
    {
    public:
        bool locked;

        SynchronizationContext( bool _locked = false )
        :
            locked( _locked )
        {
        }
    };

    typedef typename std::map<KeyType, SynchronizationContext> SyncCtxDict;

    mutable SyncCtxDict m_syncCtxDict;
    InternalContainerType m_dataDict;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
};

#endif  //SAFEPOOL_H
