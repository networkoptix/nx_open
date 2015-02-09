#include <memory>

#include "discovery_manager.h"
#include "stream_reader.h"
#include "video_packet.h"

namespace ite
{
#if 1
    DEFAULT_REF_COUNTER(StreamReader)
#else
    unsigned int StreamReader::addRef()
    {
        unsigned count = m_refManager.addRef();
        return count;
    }

    unsigned int StreamReader::releaseRef()
    {
        unsigned count = m_refManager.releaseRef();
        return count;
    }
#endif

    StreamReader::StreamReader(CameraManager * cameraManager, int encoderNumber)
    :
        m_refManager( this ),
        m_cameraManager( cameraManager ),
        m_encoderNumber( encoderNumber )
    {
        m_cameraManager->openStream(m_encoderNumber);
    }

    StreamReader::~StreamReader()
    {
        m_cameraManager->closeStream(m_encoderNumber);
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
        VideoPacket * packet = m_cameraManager->nextPacket( m_encoderNumber );
        if (!packet)
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

        packet->setTime( usecTime(packet->pts()) );
        *lpPacket = packet;
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {
    }

    //

    // TODO: PTS overflow
    uint64_t StreamReader::usecTime(PtsT pts)
    {
        static const unsigned USEC_IN_SEC = 1000 * 1000;
        static const unsigned MAX_PTS_DRIFT = 63000;    // 700 ms
        static const double timeBase = 1.0 / 9000;

        PtsT drift = pts - m_ptsPrev;
        if (drift < 0) // abs
            drift = -drift;

        if (!m_ptsPrev || drift > MAX_PTS_DRIFT)
        {
            m_time.pts = pts;
            m_time.sec = time(NULL);
        }

        m_ptsPrev = pts;
        return (m_time.sec + (pts - m_time.pts) * timeBase) * USEC_IN_SEC;
    }
}
