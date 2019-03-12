/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "archive_reader.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <cstring>

#include "dir_contents_manager.h"


ArchiveReader::ArchiveReader(
    DirContentsManager* const dirContentsManager,
    unsigned int frameDurationUsec )
:
    m_refManager( this ),
    m_dirContentsManager( dirContentsManager )
{
    m_streamReader.reset( new StreamReader(
        &m_refManager,
        m_dirContentsManager,
        frameDurationUsec,
        false,
        0 ) );
}

ArchiveReader::~ArchiveReader()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* ArchiveReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_DtsArchiveReader, sizeof(nxcip::IID_DtsArchiveReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
int ArchiveReader::addRef() const
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
int ArchiveReader::releaseRef() const
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::DtsArchiveReader::getCapabilities
unsigned int ArchiveReader::getCapabilities() const
{
    return nxcip::DtsArchiveReader::reverseGopModeCapability;
}

//!Implementation of nxcip::DtsArchiveReader::open
int ArchiveReader::open()
{
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::DtsArchiveReader::getStreamReader
nxcip::StreamReader* ArchiveReader::getStreamReader()
{
    m_streamReader->addRef();
    return m_streamReader.get();
}

//!Implementation of nxcip::DtsArchiveReader::startTime
nxcip::UsecUTCTimestamp ArchiveReader::startTime() const
{
    return m_dirContentsManager->minTimestamp();
}

//!Implementation of nxcip::DtsArchiveReader::endTime
nxcip::UsecUTCTimestamp ArchiveReader::endTime() const
{
    return m_dirContentsManager->maxTimestamp();
}

//!Implementation of nxcip::DtsArchiveReader::seek
int ArchiveReader::seek(
    unsigned int cSeq,
    nxcip::UsecUTCTimestamp timestamp,
    bool /*findKeyFrame*/,
    nxcip::UsecUTCTimestamp* selectedPosition )
{
    *selectedPosition = m_streamReader->setPosition( cSeq, timestamp );
    if( *selectedPosition == nxcip::INVALID_TIMESTAMP_VALUE )
        return nxcip::NX_NO_DATA;

    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::DtsArchiveReader::setReverseMode
int ArchiveReader::setReverseMode(
    unsigned int cSeq,
    bool isReverse,
    nxcip::UsecUTCTimestamp timestamp,
    nxcip::UsecUTCTimestamp* selectedPosition )
{
    *selectedPosition = m_streamReader->setReverseMode( cSeq, isReverse, timestamp );
    return nxcip::NX_NO_ERROR;
}

bool ArchiveReader::isReverseModeEnabled() const
{
    return m_streamReader->isReverse();
}

//!Implementation of nxcip::DtsArchiveReader::setMotionData
int ArchiveReader::setMotionDataEnabled( bool /*motionPresent*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

//!Implementation of nxcip::DtsArchiveReader::setQuality
int ArchiveReader::setQuality( nxcip::MediaStreamQuality quality, bool /*waitForKeyFrame*/ )
{
    if( quality == nxcip::msqDefault )
        return nxcip::NX_NO_ERROR;
    return nxcip::NX_NO_DATA;
}

int ArchiveReader::playRange(
    unsigned int /*cSeq*/,
    nxcip::UsecUTCTimestamp /*start*/,
    nxcip::UsecUTCTimestamp /*endTimeHint*/,
    unsigned int /*step*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

//!Implementation of nxcip::DtsArchiveReader::getLastErrorString
void ArchiveReader::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
    errorString[0] = '\0';
}
