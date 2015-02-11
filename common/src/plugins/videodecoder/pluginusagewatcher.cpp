////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "pluginusagewatcher.h"

#include <QtCore/QDateTime>
#include <utils/common/mutex.h>

#include <utils/common/log.h>


static const size_t SHARED_MEMORY_SIZE = 16*1024;

using namespace std;

PluginUsageWatcher::PluginUsageWatcher( const QString& uniquePluginID )
:
    m_uniquePluginID( uniquePluginID ),
    m_sharedMemoryLocked( false ),
    m_sharedMemoryLocker( uniquePluginID + lit("_sync") ),
    m_usageRecordID( 0 ),
    m_usageDataSharedMemory( uniquePluginID + lit("_shared") ),
    m_terminated( false )
{
    NamedMutex::ScopedLock lk( &m_sharedMemoryLocker );

    const bool created = m_usageDataSharedMemory.create( SHARED_MEMORY_SIZE );
    if( !created )
    {
        if( m_usageDataSharedMemory.error() != QSharedMemory::AlreadyExists )
        {
            NX_LOG( lit("Failed to create shared memory with key %1. %2").arg(m_uniquePluginID).arg(m_usageDataSharedMemory.errorString()), cl_logWARNING );
            return;
        }
    }

    if( !m_usageDataSharedMemory.isAttached() && !m_usageDataSharedMemory.attach() )
    {
        NX_LOG( lit("Failed to attach to shared memory with key %1. %2").arg(m_uniquePluginID).arg(m_usageDataSharedMemory.error()), cl_logWARNING );
        return;
    }

    if( created )
    {
        NX_LOG( lit("PluginUsageWatcher. Successfully created usage data shared memory region. Initializing..."), cl_logDEBUG1 );
        //initializing
        m_totalUsageArray.prevUsageRecordID = 1;
        quint8* bufStart = static_cast<quint8*>(m_usageDataSharedMemory.data());
        const quint8* bufEnd = bufStart + m_usageDataSharedMemory.size();
        if( !m_totalUsageArray.serialize( &bufStart, bufEnd ) )
        {
            NX_LOG( lit("PluginUsageWatcher. Failed to serialize usage data to shared memory region"), cl_logWARNING );
        }
    }

    start();
    if( !isRunning() )
    {
        NX_LOG( lit("PluginUsageWatcher. Failed to start internal thread").arg(m_uniquePluginID), cl_logDEBUG1 );
        return;
    }

    NX_LOG( lit("PluginUsageWatcher. Successfully attached to usage data shared memory region %1").arg(m_uniquePluginID), cl_logDEBUG1 );
}

PluginUsageWatcher::~PluginUsageWatcher()
{
    m_terminated = true;
    wait();
}

size_t PluginUsageWatcher::currentSessionCount() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return m_currentSessions.size();
}

stree::ResourceContainer PluginUsageWatcher::currentTotalUsage() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    //reading usage from shared memory
    const quint64 currentClock = QDateTime::currentMSecsSinceEpoch();

    const UsageRecord& localUsage = currentUsage();
    quint64 totalFramePictureSize = localUsage.framePictureSize;
    double totalFPS = localUsage.fps;
    quint64 totalPixelsPerSecond = localUsage.pixelsPerSecond;
    quint64 totalVideoMemoryUsage = localUsage.videoMemoryUsage;
    unsigned int simultaneousStreamCount = localUsage.simultaneousStreamCount;

    for( std::vector<UsageRecord>::size_type
        i = 0;
        i < m_totalUsageArray.records.size();
        ++i )
    {
        if( m_totalUsageArray.records[i].sequence == m_usageRecordID )
            continue;   //record belongs to us...
        if( m_totalUsageArray.records[i].updateClock + USAGE_RECORD_EXPIRE_PERIOD <= currentClock )
            continue;   //expired record. ignoring...
        totalFramePictureSize += m_totalUsageArray.records[i].framePictureSize;
        totalFPS += m_totalUsageArray.records[i].fps;
        totalPixelsPerSecond += m_totalUsageArray.records[i].pixelsPerSecond;
        totalVideoMemoryUsage += m_totalUsageArray.records[i].videoMemoryUsage;
        simultaneousStreamCount += m_totalUsageArray.records[i].simultaneousStreamCount;
    }

    stree::ResourceContainer rc;
    rc.put( DecoderParameter::framePictureSize, totalFramePictureSize );
    rc.put( DecoderParameter::fps, totalFPS );
    rc.put( DecoderParameter::pixelsPerSecond, totalPixelsPerSecond );
    rc.put( DecoderParameter::videoMemoryUsage, totalVideoMemoryUsage );
    rc.put( DecoderParameter::simultaneousStreamCount, simultaneousStreamCount );
    return rc;
}

void PluginUsageWatcher::decoderCreated( stree::AbstractResourceReader* const decoder )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    m_currentSessions.insert( decoder );
}

void PluginUsageWatcher::decoderIsAboutToBeDestroyed( stree::AbstractResourceReader* const decoder )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    m_currentSessions.erase( decoder );

    //TODO/IMPL update usage record
}

bool PluginUsageWatcher::readExternalUsage()
{
    m_totalUsageArray.records.clear();
    return readExternalUsage( &m_totalUsageArray );
}

bool PluginUsageWatcher::updateUsageParams()
{
    UsageRecord localUsage = currentUsage();

    //assuming that m_totalUsageArray has been read under current lock
    if( !m_usageRecordID )
    {
        m_usageRecordID = ++m_totalUsageArray.prevUsageRecordID;
        NX_LOG( lit("PluginUsageWatcher. Selected usage record id %1").arg(m_usageRecordID), cl_logDEBUG1 );
    }
    localUsage.sequence = m_usageRecordID;
    localUsage.updateClock = QDateTime::currentMSecsSinceEpoch();
    m_totalUsageArray.records.push_back( localUsage );

    //writing current session usage to shared memory
    quint8* bufStart = static_cast<quint8*>(m_usageDataSharedMemory.data());
    const quint8* bufEnd = bufStart + m_usageDataSharedMemory.size();
    if( !m_totalUsageArray.serialize( &bufStart, bufEnd ) )
    {
        NX_LOG( lit("PluginUsageWatcher. Failed to serialize usage data to shared memory region"), cl_logWARNING );
        return false;
    }

    return true;
}

void PluginUsageWatcher::run()
{
    while( !m_terminated )
    {
        {
            NamedMutex::ScopedLock lk( &m_sharedMemoryLocker );

            if( m_usageRecordID > 0 )
            {
                //updating usage record so that it not expired
                readExternalUsage();
                updateUsageParams();
            }
        }

        msleep( USAGE_RECORD_EXPIRE_PERIOD / 2 );
    }
}

bool PluginUsageWatcher::lockSharedMemory()
{
    Q_ASSERT( !m_sharedMemoryLocked );
    m_sharedMemoryLocked = m_sharedMemoryLocker.lock();
    if( !m_sharedMemoryLocked )
    {
        NX_LOG( lit("PluginUsageWatcher. Failed to lock semaphore. id %1. error: %2").
            arg(m_uniquePluginID).arg(m_sharedMemoryLocker.errorString()), cl_logERROR );
    }
    return m_sharedMemoryLocked;
}

bool PluginUsageWatcher::unlockSharedMemory()
{
    Q_ASSERT( m_sharedMemoryLocked );
    m_sharedMemoryLocked = !m_sharedMemoryLocker.unlock();
    if( m_sharedMemoryLocked )
    {
        //TODO/IMPL what to do in this case?
        NX_LOG( lit("PluginUsageWatcher. Failed to release semaphore. id %1. error: %2").
            arg(m_uniquePluginID).arg(m_sharedMemoryLocker.errorString()), cl_logERROR );
    }
    return !m_sharedMemoryLocked;
}

bool PluginUsageWatcher::readExternalUsage( UsageRecordArray* const recArray ) const
{
    const quint8* bufStart = static_cast<const quint8*>(m_usageDataSharedMemory.constData());
    const quint8* bufEnd = bufStart + m_usageDataSharedMemory.size();
    if( !recArray->deserialize( &bufStart, bufEnd ) )
    {
        NX_LOG( lit("PluginUsageWatcher. Failed to deserialize usage data from shared memory region"), cl_logWARNING );
        return false;
    }

    NX_LOG( lit("PluginUsageWatcher. Read %1 usage records").arg(recArray->records.size()), cl_logDEBUG2 );

    return true;
}

UsageRecord PluginUsageWatcher::currentUsage() const
{
    UsageRecord rec;
    for( std::set<stree::AbstractResourceReader*>::const_iterator
        it = m_currentSessions.begin();
        it != m_currentSessions.end();
        ++it )
    {
        unsigned int framePictureSize = 0;
        if( (*it)->getTypedVal( DecoderParameter::framePictureSize, &framePictureSize ) )
            rec.framePictureSize += framePictureSize;

        double fps = 0;
        if( (*it)->getTypedVal( DecoderParameter::fps, &fps ) )
            rec.fps += fps;

        quint64 pixelsPerSecond = 0;
        if( (*it)->getTypedVal( DecoderParameter::pixelsPerSecond, &pixelsPerSecond ) )
            rec.pixelsPerSecond += pixelsPerSecond;

        quint64 videoMemoryUsage = 0;
        if( (*it)->getTypedVal( DecoderParameter::videoMemoryUsage, &videoMemoryUsage ) )
            rec.videoMemoryUsage += videoMemoryUsage;
    }
    rec.simultaneousStreamCount = m_currentSessions.size();

    return rec;
}
