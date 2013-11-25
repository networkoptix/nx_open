/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <memory>

#include "ilp_video_packet.h"
#include "ilp_empty_packet.h"
#include "motion_data_picture.h"

#define GENERATE_RANDOM_MOTION
#ifdef GENERATE_RANDOM_MOTION
static const unsigned int MOTION_PRESENCE_CHANCE_PERCENT = 70;
#endif


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    DirContentsManager* const dirContentsManager,
    unsigned int frameDurationUsec,
    bool liveMode )
:
    m_refManager( parentRefManager ),
    m_encoderNumber( 0 ),
    m_curTimestamp( 0 ),
    m_frameDuration( frameDurationUsec ),
    m_nextFrameDeployTime( 0 ),
    m_isReverse( false ),
    m_cSeq( 0 )
{
}

StreamReader::~StreamReader()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
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
unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}
