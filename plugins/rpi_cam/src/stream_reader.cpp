#include "media_encoder.h"
#include "stream_reader.h"

namespace rpi_cam
{
    DEFAULT_REF_COUNTER(StreamReader)

    StreamReader::StreamReader(MediaEncoder * encoder)
    :   m_refManager(encoder->refManager()),
        encoder_(encoder)
    {}

    StreamReader::~StreamReader()
    {}

    void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_StreamReader)
        {
            addRef();
            return this;
        }

        return nullptr;
    }

    int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
    {
        nxcip::MediaDataPacket * packet = encoder_->nextFrame();
        if (!packet)
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }


        *lpPacket = packet;
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {}
}
