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

#include <core/misc/schedule_task.h>

#include "motion_window.h"


//!Contains camera settings usually modified by user
/*!
    E.g., recording schedule, motion type, second stream quality, etc...
*/
class QnUserCameraSettings
{
public:
    Qn::MotionType motionType;
    QList<QnMotionRegion> motionRegions;
    bool scheduleDisabled;
    bool audioEnabled;
    bool cameraControlDisabled;
    QnScheduleTaskList scheduleTasks;

    QnUserCameraSettings();
};


//!Pool of \a QnUserCameraSettings
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
            m_lockedElement( QnCameraUserAttributePool::instance()->lock( key ) )
        {
        }
        ~ScopedLock()
        {
            m_lockedElement = nullptr;
            QnCameraUserAttributePool::instance()->unlock( m_key );
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

    //!If element already locked, blocks till element has been unlocked
    /*!
        \note if element with id \a resID does not exist, it is created
    */
    MappedType* lock( const KeyType& key )
    {
        QMutexLocker lk( &m_mutex );
        auto p = m_elements.emplace( std::make_pair( key, nullptr ) );
        if( p.second )
            p.first->second.reset( new DataCtx() );
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

class QnCameraUserAttributePool
:
    public QnGeneralAttributePool<QnUuid, QnUserCameraSettings>
{
public:
    QnCameraUserAttributePool();
    virtual ~QnCameraUserAttributePool();

    static QnCameraUserAttributePool* instance();
};

#endif  //CAMERA_USER_ATTRIBUTE_POOL_H
