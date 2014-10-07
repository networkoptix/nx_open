/**********************************************************
* 1 oct 2014
* akolesnikov
***********************************************************/

#ifndef CAMERA_USER_ATTRIBUTE_POOL_H
#define CAMERA_USER_ATTRIBUTE_POOL_H

#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>
#include <QtCore/QSharedPointer>

#include "camera_user_attributes.h"
#include "resource_fwd.h"


//!Pool of \a QnCameraUserAttributes
/*!
    Example of adding element to pool and setting some values:
    \code {*.cpp}
    QnCameraUserAttributePool::ScopedLock lk( QnCameraUserAttributePool::instance(), resourceID );
    if( !lk->isAudioEnabled() )
        lk->setAudioEnabled( true );
    lk->setCameraControlDisabled( true );
    \endcode

    \a QnCameraUserAttributePool::ScopedLock constructor will block if element with id \a resourceID is already used
*/
template<class KeyType, class MappedType>
class QnGeneralAttributePool
{
public:
    class ScopedLock
    {
    public:
        ScopedLock( QnGeneralAttributePool* pool, const KeyType& key )
        :
            m_pool( pool ),
            m_key( key ),
            m_lockedElement( pool->lock( key ) )
        {
        }
        ~ScopedLock()
        {
            m_lockedElement = nullptr;
            m_pool->unlock( m_key );
        }

        MappedType* operator->() { return m_lockedElement; }
        const MappedType* operator->() const { return m_lockedElement; }

        MappedType& operator*() { return *m_lockedElement; }
        const MappedType& operator*() const { return *m_lockedElement; }

    private:
        QnGeneralAttributePool* m_pool;
        const KeyType m_key;
        MappedType* m_lockedElement;
    };

    template<class T>
    void setElementInitializer( T handler )
    {
        m_customInitializer = std::move(handler);
    }

    MappedType get( const KeyType& key )
    {
        ScopedLock lk( this, key );
        return *lk;
    }

private:
    class DataCtx
    {
    public:
        DataCtx()
        :
            locked(false)
        {
        }

        bool locked;
        MappedType mapped;
    };

    std::map<KeyType, std::unique_ptr<DataCtx>> m_elements;
    QMutex m_mutex;
    QWaitCondition m_cond;
    std::function<void(const KeyType&, MappedType&)> m_customInitializer;

    //!If element already locked, blocks till element has been unlocked
    /*!
        \note if element with id \a resID does not exist, it is created
    */
    MappedType* lock( const KeyType& key )
    {
        QMutexLocker lk( &m_mutex );
        auto p = m_elements.emplace( std::make_pair( key, nullptr ) );
        if( p.second )
        {
            p.first->second.reset( new DataCtx() );
            if( m_customInitializer )
                m_customInitializer( key, p.first->second->mapped );
        }
        while( p.first->second->locked )
            m_cond.wait( lk.mutex() );
        p.first->second->locked = true;
        return &p.first->second->mapped;
    }

    void unlock( const KeyType& key )
    {
        QMutexLocker lk( &m_mutex );
        auto it = m_elements.find( key );
        assert( it != m_elements.end() );
        assert( it->second->locked );
        it->second->locked = false;
        m_cond.wakeAll();
    }
};

template<class ResourcePtrType>
QList<QnUuid> idListFromResList( const QList<ResourcePtrType>& resList )
{
    QList<QnUuid> idList;
    idList.reserve( resList.size() );
    for( const ResourcePtrType& resPtr: resList )
        idList.push_back( resPtr->getId() );
    return idList;
}

class QnCameraUserAttributePool
:
    public QnGeneralAttributePool<QnUuid, QnCameraUserAttributesPtr>
{
public:
    QnCameraUserAttributePool();
    virtual ~QnCameraUserAttributePool();

    QnCameraUserAttributesList getAttributesList( const QList<QnUuid>& idList );

    static QnCameraUserAttributePool* instance();
};

#endif  //CAMERA_USER_ATTRIBUTE_POOL_H
