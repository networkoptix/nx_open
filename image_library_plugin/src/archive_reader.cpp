/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "archive_reader.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <cstring>

#include "settings.h"


ArchiveReader::ArchiveReader( const std::string& pictureDirectoryPath )
:
    m_refManager( this ),
    m_pictureDirectoryPath( pictureDirectoryPath )
{
    m_streamReader.reset( new StreamReader( this, pictureDirectoryPath, Settings::instance()->frameDurationUsec ) );
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
unsigned int ArchiveReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int ArchiveReader::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::DtsArchiveReader::getCapabilities
unsigned int ArchiveReader::getCapabilities() const
{
    return nxcip::DtsArchiveReader::reverseModeCapability;
}

//!Implementation of nxcip::DtsArchiveReader::open
bool ArchiveReader::open()
{
    //touching directory
    struct stat st;
    memset( &st, 0, sizeof(st) );
    return stat( m_pictureDirectoryPath.c_str(), &st ) == 0;
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
    //TODO/IMPL
    return 0;
}

//!Implementation of nxcip::DtsArchiveReader::endTime
nxcip::UsecUTCTimestamp ArchiveReader::endTime() const
{
    //TODO/IMPL
    return 0;
}

//!Implementation of nxcip::DtsArchiveReader::seek
int ArchiveReader::seek( nxcip::UsecUTCTimestamp /*timestamp*/, bool /*findKeyFrame*/, nxcip::UsecUTCTimestamp* /*selectedPosition*/ )
{
    //TODO/IMPL
    return nxcip::NX_NOT_IMPLEMENTED;
}

//!Implementation of nxcip::DtsArchiveReader::setReverseMode
int ArchiveReader::setReverseMode( bool /*isReverse*/, nxcip::UsecUTCTimestamp /*timestamp*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

//!Implementation of nxcip::DtsArchiveReader::setMotionData
int ArchiveReader::setMotionData( bool /*motionPresent*/ )
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

//!Implementation of nxcip::DtsArchiveReader::find
int ArchiveReader::find( nxcip::MotionData* /*motionMask*/, nxcip::TimePeriods** /*timePeriods*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

//!Implementation of nxcip::DtsArchiveReader::getLastErrorString
void ArchiveReader::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
    errorString[0] = '\0';
}

CommonRefManager* ArchiveReader::refManager()
{
    return &m_refManager;
}
