#include <memory>

#include "discovery_manager.h"
#include "camera_manager.h"
#include "video_packet.h"
#include "dev_reader.h"
#include "stream_reader.h"

namespace ite {
namespace aux {

// FramesFrequencyWatcher --------------------------------------------------------------------------
class FramesFrequencyWatcher
{
public:
    FramesFrequencyWatcher(int maxFramesPerSecond);
    void gotFrame();
    bool isOverflow() const;

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    const int m_maxFramesPerSecond;
    int m_frameCount = 0;
    TimePoint m_lastTimePoint;
    mutable bool m_overflow = false;
};

FramesFrequencyWatcher::FramesFrequencyWatcher(int maxFramesPerSecond):
    m_maxFramesPerSecond(maxFramesPerSecond),
    m_lastTimePoint(TimePoint::max())
{
}

void FramesFrequencyWatcher::gotFrame()
{
    using namespace std::chrono;

    auto now = Clock::now();
    if (m_lastTimePoint == TimePoint::max())
    {
        m_lastTimePoint = now;
        m_frameCount = 1;
        return;
    }

    m_frameCount++;

    auto timeSinceLastFrame = duration_cast<milliseconds>(now - m_lastTimePoint);
    if (timeSinceLastFrame < seconds(1))
        return;

    auto framesPerSec = (m_frameCount * 1000) / timeSinceLastFrame.count();
    if (framesPerSec > m_maxFramesPerSecond)
    {
        std::cout << "ITE930_WARNING: Frames per second overflow detected: "  << framesPerSec << std::endl;
        m_overflow = true;
    }

    m_lastTimePoint = now;
    m_frameCount = 0;
}

bool FramesFrequencyWatcher::isOverflow() const
{
    bool oldVal = m_overflow;
    m_overflow = false;
    return oldVal;
}
// -------------------------------------------------------------------------------------------------

} // namespace aux


// StreamReader ------------------------------------------------------------------------------------

static DevReader * getDevReader(CameraManager * camera)
{
    auto rxDevice = camera->rxDevice().lock();
    if (rxDevice)
        return rxDevice->reader();
    return nullptr;
}

StreamReader::StreamReader(CameraManager * cameraManager, int encoderNumber)
:   m_cameraManager(cameraManager),
    m_encoderNumber(encoderNumber),
    m_interrupted(false)

{
    m_cameraManager->openStream(m_encoderNumber);
    if (settings.maxFramesPerSecond < 0)
        return;

    m_freqWatcher.reset(new aux::FramesFrequencyWatcher(settings.maxFramesPerSecond));
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
        printf("[stream] Cam: %d; no data\n", m_cameraManager->cameraId());
        return nxcip::NX_NO_DATA;
    }

#if 0
    debug_printf("[stream] pts: %u; time: %lu; stamp: %lu\n", pkt->pts(), time(NULL), pkt->timestamp() / 1000 / 1000);
#endif

    VideoPacket * packet = new VideoPacket(pkt->data(), pkt->size(), pkt->timestamp());
    if (!packet)
    {
        *lpPacket = nullptr;
        debug_printf("[stream] no VideoPacket\n");
        return nxcip::NX_TRY_AGAIN;
    }
#if 0
    debug_printf(
        "[stream [%d] ] ts:\t %llu diff:\t %llu\n",
        m_encoderNumber,
        packet->timestamp(),
        packet->timestamp() - m_ts
    );
    m_ts = packet->timestamp();
#endif

    if (pkt->flags() & ContentPacket::F_StreamReset)
        packet->setFlag(nxcip::MediaDataPacket::fStreamReset);

    *lpPacket = packet;
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    std::lock_guard<std::mutex> lock(m_cameraManager->get_mutex());
    if (!m_cameraManager->rxDeviceRef())
        return;

    DevReader * devReader = getDevReader(m_cameraManager);
    if (devReader)
    {
        m_interrupted = true;
        devReader->interrupt();
    }
}

//

bool StreamReader::framesTooOften() const
{
    if (!m_freqWatcher)
        return false;

    m_freqWatcher->gotFrame();
    return m_freqWatcher->isOverflow();
}

ContentPacketPtr StreamReader::nextPacket()
{
    static const unsigned TIMEOUT_MS = 10000;
    std::chrono::milliseconds timeout(TIMEOUT_MS);

    Timer timer(true);
    while (timer.elapsedMS() < TIMEOUT_MS)
    {
        //std::lock_guard<std::mutex> lock(m_cameraManager->get_mutex());
        if (!m_cameraManager->rxDeviceRef())
            return nullptr;

        DevReader * devReader = getDevReader(m_cameraManager);
        if (!devReader || !devReader->hasThread() || !m_cameraManager->rxDeviceRef()->deviceReady())
            return nullptr;

        ContentPacketPtr pkt = devReader->getPacket(RxDevice::stream2pid(m_encoderNumber), timeout);
        if (m_interrupted)
            break;

        if (pkt && framesTooOften())
            return nullptr;

        if (pkt)
            return pkt;

        auto rxDev = m_cameraManager->rxDevice().lock();
        if (rxDev)
            rxDev->processRcQueue();
    }

    m_interrupted = false;
    return nullptr;
}

} // namespace ite
