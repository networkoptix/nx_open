#include <memory>

#include "discovery_manager.h"
#include "camera_manager.h"
#include "video_packet.h"
#include "dev_reader.h"
#include "stream_reader.h"

namespace ite
{
    INIT_OBJECT_COUNTER(StreamReader)
    DEFAULT_REF_COUNTER(StreamReader)

    static DevReader * getDevReader(CameraManager * camera)
    {
        auto rxDevice = camera->rxDevice().lock();
        if (rxDevice)
            return rxDevice->reader();
        return nullptr;
    }

    StreamReader::StreamReader(CameraManager * cameraManager, int encoderNumber)
    :   m_refManager(this),
        m_cameraManager(cameraManager),
        m_encoderNumber(encoderNumber),
        m_interrupted(false)
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
        ContentPacketPtr pkt = nextPacket();
        if (!pkt || pkt->empty())
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

#if 0
        debug_printf("[stream] pts: %u; time: %lu; stamp: %lu\n", pkt->pts(), time(NULL), pkt->timestamp() / 1000 / 1000);
#endif

        VideoPacket * packet = new VideoPacket(pkt->data(), pkt->size(), pkt->timestamp());
        if (!packet)
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

        if (pkt->flags() & ContentPacket::F_StreamReset)
            packet->setFlag(nxcip::MediaDataPacket::fStreamReset);

        *lpPacket = packet;
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {
        DevReader * devReader = getDevReader(m_cameraManager);
        if (devReader)
        {
            m_interrupted = true;
            devReader->interrupt();
        }
    }

    //

    ContentPacketPtr StreamReader::nextPacket()
    {
        static const unsigned TIMEOUT_MS = 10000;
        std::chrono::milliseconds timeout(TIMEOUT_MS);

        Timer timer(true);
        while (timer.elapsedMS() < TIMEOUT_MS)
        {
            DevReader * devReader = getDevReader(m_cameraManager);
            if (! devReader)
                break;

            ContentPacketPtr pkt = devReader->getPacket(RxDevice::stream2pid(m_encoderNumber), timeout);
            if (m_interrupted)
                break;

            if (pkt)
                return pkt;

            auto rxDev = m_cameraManager->rxDevice().lock();
            if (rxDev)
                rxDev->processRcQueue();
        }

        m_interrupted = false;
        return nullptr;
    }
}
