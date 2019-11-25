#include "fan_platform_abstraction.h"

#include <chrono>

#include <nx/utils/log/log.h>
#include <nx/utils/elapsed_timer.h>

#include <nx/vms/server/nvr/hanwha/ioctl_common.h>

namespace nx::vms::server::nvr::hanwha {

#if defined(Q_OS_LINUX)

class FanPlatformAbstractionImpl: public IFanPlatformAbstraction
{
public:
    FanPlatformAbstractionImpl(int ioDeviceDescriptor):
        m_ioDeviceDescriptor(ioDeviceDescriptor)
    {
        NX_DEBUG(this, "Creating the fan platform abstraction implementation");
    }

    virtual ~FanPlatformAbstractionImpl() override
    {
        NX_DEBUG(this, "Destroying the fan platform abstraction implementation");
    }

    virtual FanState state() const override
    {
        NX_DEBUG(this, "Fan state has been requested");

        static const std::chrono::seconds kWaitTimeout(1);

        const int command = prepareCommand<int>(kGetFanAlarmCommand);
        int commandData = 1;

        if (const int result = ::ioctl(m_ioDeviceDescriptor, command, &commandData); result != 0)
        {
            NX_WARNING(this,
                "Unable to execute the first stage of fan alarm check, error: %1", result);
            return FanState::error;
        }

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            nx::utils::ElapsedTimer timer;
            timer.restart();
            while (!m_interrupted && timer.elapsed() < kWaitTimeout)
            {
                m_waitCondition.wait(
                    &m_mutex,
                    std::max(std::chrono::milliseconds::zero(), kWaitTimeout - timer.elapsed()));
            }

            if (m_interrupted)
            {
                NX_DEBUG(this, "Platform abstraction has been interrupted, exiting");
                return FanState::undefined;
            }
        }

        commandData = 0;
        if (const int result = ::ioctl(m_ioDeviceDescriptor, command, &commandData); result == 0)
        {
            NX_WARNING(this, "Fan error detected, result: %1", result);
            return FanState::error;
        }

        return FanState::ok;
    }

    virtual void interrupt() override
    {
        NX_DEBUG(this, "Terminating");
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_interrupted = true;
        m_waitCondition.wakeOne();
    }

private:
    int m_ioDeviceDescriptor = -1;

    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::WaitCondition m_waitCondition;

    bool m_interrupted = false;
};

#endif

FanPlatformAbstractionImpl* createPlatformAbstractionImpl(int ioDeviceDescriptor)
{
#if defined(Q_OS_LINUX)
    return new FanPlatformAbstractionImpl(ioDeviceDescriptor);
#else
    return nullptr;
#endif
}

FanPlatformAbstraction::FanPlatformAbstraction(int ioDeviceDescriptor):
    m_impl(createPlatformAbstractionImpl(ioDeviceDescriptor))
{
    NX_DEBUG(this, "Creating the fan platform abstraction");
}

FanPlatformAbstraction::~FanPlatformAbstraction()
{
    NX_DEBUG(this, "Destroying the fan platform abstraction");
}

FanState FanPlatformAbstraction::state() const
{
    if (NX_ASSERT(m_impl))
        return m_impl->state();

    return FanState::undefined;
}

void FanPlatformAbstraction::interrupt()
{
    if (NX_ASSERT(m_impl))
        m_impl->interrupt();
}

} // namespace nx::vms::server::nvr::hanwha
