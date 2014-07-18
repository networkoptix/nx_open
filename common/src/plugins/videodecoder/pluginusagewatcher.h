////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PLUGINUSAGEWATCHER_H
#define PLUGINUSAGEWATCHER_H

#include <set>

#include <QtCore/QMutex>
#include <QSharedMemory>

#include "pluginusagerecord.h"
#include "videodecoderplugintypes.h"
#include "../../decoders/video/abstractdecoder.h"
#include "../../utils/ipc/namedmutex.h"


class UsageRecordArray;

//!Monitors usage of video decoder plugin by all processes, using it
/*!
    Provides list of streams, decoded by plugin with stream parameters:\n
    - picture resolution
    - codec profile
    - current fps

    \note Methods of this class are thread-safe
*/
class PluginUsageWatcher
:
    public QThread
{
public:
    class ScopedLock
    {
    public:
        //!Calls usageWatcher->lockSharedMemory()
        ScopedLock( PluginUsageWatcher* const usageWatcher )
        :
            m_usageWatcher( usageWatcher ),
            m_locked( false )
        {
            m_locked = m_usageWatcher->lockSharedMemory();
        }
        //!Calls usageWatcher->unlockSharedMemory()
        ~ScopedLock()
        {
            if( m_locked )
                m_usageWatcher->unlockSharedMemory();
        }

        //!Returns true, if locked semaphore. false, otherwise
        bool locked() const
        {
            return m_locked;
        }

    private:
        PluginUsageWatcher* const m_usageWatcher;
        bool m_locked;
    };

    /*!
        \param uniquePluginID ID used to distinguish different plugins on system. It is recommended that UID is found here
    */
    PluginUsageWatcher( const QString& uniquePluginID );
    virtual ~PluginUsageWatcher();

    //!Returns number of current decode sessions, using this plugin
    size_t currentSessionCount() const;
    //!Returns parameters, describing total plugin usage (total pixels per second, total fps, etc...)
    /*!
        Method returns following resources:\n
            - framePictureSize
            - fps
            - pixelsPerSecond
            - videoMemoryUsage
        Every value is a sum of values of all active sessions
        \note Returned data is actual for the moment of calling this method
        \note Lock on shared memory is required for calling this method
    */
    stree::ResourceContainer currentTotalUsage() const;
    //!Add decoder \a decoder to the list of current sessions
    /*!
        Before \a decoder destruction one MUST call method \a decoderIsAboutToBeDestroyed
    */
    void decoderCreated( stree::AbstractResourceReader* const decoder );
    //!Removes \a decoder from current session list
    void decoderIsAboutToBeDestroyed( stree::AbstractResourceReader* const decoder );

    //!Locks shared memory from being used by other processes
    bool lockSharedMemory();
    //!Unlocks shared memory locked by \a lockSharedMemory
    bool unlockSharedMemory();
    //!Reads plugin usage by other processes from shared memory
    /*!
        \return true on success
        \note This method MUST be protected with \a lockSharedMemory
    */
    bool readExternalUsage();
    //!Updates usage info in shared memory to make it available to other proceses running same plugin
    /*!
        \note This method MUST be protected with \a lockSharedMemory
        \return true, if data has been successfully written
    */
    bool updateUsageParams();

protected:
    virtual void run();

private:
    mutable QMutex m_mutex;
    QString m_uniquePluginID;
    std::set<stree::AbstractResourceReader*> m_currentSessions;
    bool m_sharedMemoryLocked;
    //!Used to synchronize access to shared memory. We do not use QSharedMemory internal semaphore, because it can dead-lock on ms windows in case of unexpected process termination
    NamedMutex m_sharedMemoryLocker;
    size_t m_usageRecordID;
    UsageRecordArray m_totalUsageArray;
    QSharedMemory m_usageDataSharedMemory;
    bool m_terminated;

    bool readExternalUsage( UsageRecordArray* const recArray ) const;
    UsageRecord currentUsage() const;
};

#endif  //PLUGINUSAGEWATCHER_H
