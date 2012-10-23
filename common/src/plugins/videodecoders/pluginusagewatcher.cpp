////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "pluginusagewatcher.h"

#include <QMutexLocker>


using namespace std;

PluginUsageWatcher::PluginUsageWatcher( const QByteArray& uniquePluginID )
:
    m_uniquePluginID( uniquePluginID )
{
}

//!Returns list of current decode sessions, using this plugin
std::set<stree::AbstractResourceReader*> PluginUsageWatcher::currentSessions() const
{
    QMutexLocker lk( &m_mutex );

    //TODO/IMPL checking whether local data is actual
        //if not, locking shared memory
        //reading data from shared memory

    //how to check that data in shared memory is actual?
        //e.g. process has been terminated and it didn't have a chance to clean after itself
        //process id cannot be relied on, since it can be used by newly created process

    //TODO/IMPL
    return m_currentSessions;
}

size_t PluginUsageWatcher::currentSessionCount() const
{
    QMutexLocker lk( &m_mutex );
    return m_currentSessions.size();
}

stree::ResourceContainer PluginUsageWatcher::currentTotalUsage() const
{
    QMutexLocker lk( &m_mutex );

    quint64 totalFramePictureSize = 0;
    double totalFPS = 0;
    quint64 totalPixelsPerSecond = 0;
    quint64 totalVideoMemoryUsage = 0;
    for( std::set<stree::AbstractResourceReader*>::const_iterator
        it = m_currentSessions.begin();
        it != m_currentSessions.end();
        ++it )
    {
        unsigned int framePictureSize = 0;
        if( (*it)->getTypedVal( DecoderParameter::framePictureSize, &framePictureSize ) )
            totalFramePictureSize += framePictureSize;

        double fps = 0;
        if( (*it)->getTypedVal( DecoderParameter::fps, &fps ) )
            totalFPS += fps;

        quint64 pixelsPerSecond = 0;
        if( (*it)->getTypedVal( DecoderParameter::pixelsPerSecond, &pixelsPerSecond ) )
            totalPixelsPerSecond += pixelsPerSecond;

        quint64 videoMemoryUsage = 0;
        if( (*it)->getTypedVal( DecoderParameter::videoMemoryUsage, &videoMemoryUsage ) )
            totalVideoMemoryUsage += videoMemoryUsage;
    }

    stree::ResourceContainer rc;
    rc.put( DecoderParameter::framePictureSize, totalFramePictureSize );
    rc.put( DecoderParameter::fps, totalFPS );
    rc.put( DecoderParameter::pixelsPerSecond, totalPixelsPerSecond );
    rc.put( DecoderParameter::videoMemoryUsage, totalVideoMemoryUsage );
    rc.put( DecoderParameter::simultaneousStreamCount, m_currentSessions.size() );
    return rc;
}

void PluginUsageWatcher::decoderCreated( stree::AbstractResourceReader* const decoder )
{
    QMutexLocker lk( &m_mutex );

    m_currentSessions.insert( decoder );
}

void PluginUsageWatcher::decoderIsAboutToBeDestroyed( stree::AbstractResourceReader* const decoder )
{
    QMutexLocker lk( &m_mutex );

    m_currentSessions.erase( decoder );
}
