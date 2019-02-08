/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "dir_contents_manager.h"

#include <sys/timeb.h>
#include <sys/types.h>
#include <cstring>

#include "dir_iterator.h"


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
static const unsigned int MAX_IMAGES_IN_ARCHIVE = 10000;

DirContentsManager::DirContentsManager(
    const std::string& imageDir,
    unsigned int frameDurationUsec )
:
    m_imageDir( imageDir ),
    m_frameDurationUsec( frameDurationUsec )
{
    readDirContents();
}

/*!
    \return map<timestamp, file full path>
*/
std::map<nxcip::UsecUTCTimestamp, std::string> DirContentsManager::dirContents() const
{
    return m_dirContents;
}

void DirContentsManager::add( const nxcip::UsecUTCTimestamp& timestamp, const std::string& filePath )
{
    Mutex::ScopedLock lk( &m_mutex );

    m_dirContents.insert( std::make_pair( timestamp, filePath ) );
    while( m_dirContents.size() > MAX_IMAGES_IN_ARCHIVE )
        m_dirContents.erase( m_dirContents.begin() );
}

nxcip::UsecUTCTimestamp DirContentsManager::minTimestamp() const
{
    Mutex::ScopedLock lk( &m_mutex );

    if( m_dirContents.empty() )
        return nxcip::INVALID_TIMESTAMP_VALUE;
    return m_dirContents.begin()->first;
}

nxcip::UsecUTCTimestamp DirContentsManager::maxTimestamp() const
{
    Mutex::ScopedLock lk( &m_mutex );

    if( m_dirContents.empty() )
        return nxcip::INVALID_TIMESTAMP_VALUE;
    return m_dirContents.rbegin()->first;
}

void DirContentsManager::readDirContents()
{
    DirIterator dirIterator( m_imageDir );

    dirIterator.setRecursive( true );
    dirIterator.setEntryTypeFilter( FsEntryType::etRegularFile );
    dirIterator.setWildCardMask( "*.jp*g" );

    struct timeb curTime;
    memset( &curTime, 0, sizeof(curTime) );
    ftime( &curTime );
    const nxcip::UsecUTCTimestamp lastPicTimestamp = curTime.time * USEC_IN_SEC + curTime.millitm * USEC_IN_MS;

    std::map<nxcip::UsecUTCTimestamp, std::string> newDirContents;

    //reading directory
    for( nxcip::UsecUTCTimestamp
        curTimestamp = lastPicTimestamp;
        dirIterator.next();
        curTimestamp -= m_frameDurationUsec )
    {
        newDirContents.insert( std::make_pair( curTimestamp, dirIterator.entryFullPath() ) );
    }

    {
        Mutex::ScopedLock lk( &m_mutex );
        m_dirContents = newDirContents;
    }
}
