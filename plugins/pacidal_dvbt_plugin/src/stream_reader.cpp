#include <memory>

#include "discovery_manager.h"
#include "stream_reader.h"
#include "video_packet.h"

namespace ite
{
    DEFAULT_REF_COUNTER(StreamReader)

    StreamReader::StreamReader(nxpt::CommonRefManager* refManager, CameraManager* cameraManager, int encoderNumber)
    :
        m_refManager( refManager ),
        m_cameraManager( cameraManager ),
        m_encoderNumber( encoderNumber )
    {
        m_cameraManager->openStream(m_encoderNumber);
    }

    StreamReader::~StreamReader()
    {
    }

    void * StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_StreamReader)
        {
            addRef();
            return this;
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
    {
        nxcip::MediaDataPacket * packet = m_cameraManager->nextPacket( m_encoderNumber );
        if (!packet)
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

        *lpPacket = packet;
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {
        m_cameraManager->closeStream(m_encoderNumber);
    }
}
