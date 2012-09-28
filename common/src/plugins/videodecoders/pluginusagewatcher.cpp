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
std::list<DecoderStreamDescription> PluginUsageWatcher::currentSessions() const
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

stree::ResourceContainer PluginUsageWatcher::currentTotalUsage() const
{
    QMutexLocker lk( &m_mutex );

    stree::ResourceContainer rc;
    for( DecoderToSessionDescriptionType::const_iterator
        it = m_decoderToSessionDescription.begin();
        it != m_decoderToSessionDescription.end();
        ++it )
    {
        //TODO/IMPL
    }

    return rc;
}

void PluginUsageWatcher::decoderCreated( QnAbstractVideoDecoder* const decoder )
{
    QMutexLocker lk( &m_mutex );

    DecoderStreamDescription desc;
    //TODO/IMPL filling DecoderStreamDescription
    desc.put( DecoderParameter::framePictureSize, decoder->getWidth()*decoder->getHeight() );

    std::pair<DecoderToSessionDescriptionType::iterator, bool>
        p = m_decoderToSessionDescription.insert( std::make_pair( decoder, m_currentSessions.end() ) );
    if( !p.second )
        return; //such decoder already registered
    m_currentSessions.push_back( desc );
    p.first->second = --m_currentSessions.end();
}

void PluginUsageWatcher::decoderIsAboutToBeDestroyed( QnAbstractVideoDecoder* const decoder )
{
    QMutexLocker lk( &m_mutex );

    DecoderToSessionDescriptionType::iterator it = m_decoderToSessionDescription.find( decoder );
    if( it == m_decoderToSessionDescription.end() )
        return; //decoder not found
    m_currentSessions.erase( it->second );
    m_decoderToSessionDescription.erase( it );
}
