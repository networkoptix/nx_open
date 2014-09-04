#include <memory>

#include "discovery_manager.h"
#include "stream_reader.h"
#include "video_packet.h"

namespace pacidal
{
    DEFAULT_REF_COUNTER(StreamReader)

    StreamReader::StreamReader( CameraManager* cameraManager, int encoderNumber )
    :
        m_refManager( DiscoveryManager::refManager() ),
        m_cameraManager( cameraManager ),
        m_encoderNumber( encoderNumber )
    {
    }

    StreamReader::~StreamReader()
    {
    }

    //!Implementation of nxpl::PluginInterface::queryInterface
    void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if( interfaceID == nxcip::IID_StreamReader )
        {
            addRef();
            return this;
        }
        if( interfaceID == nxpl::IID_PluginInterface )
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
    {
        *lpPacket = m_cameraManager->nextPacket( m_encoderNumber );
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {
    }
}
