#include "buzzer_platform_abstraction.h"

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/ioctl_common.h>

namespace nx::vms::server::nvr::hanwha {

#if defined (Q_OS_LINUX)

class BuzzerPlatformAbstractionImpl: public IBuzzerPlatformAbstraction
{
public:
    BuzzerPlatformAbstractionImpl(int ioDeviceDescriptor):
        m_ioDeviceDescriptor(ioDeviceDescriptor)
    {
        NX_DEBUG(this, "Creating the buzzer platform abstraction implementation");
    }

    virtual ~BuzzerPlatformAbstractionImpl() override
    {
        NX_DEBUG(this, "Destroying the buzzer platform abstraction implementation");
    }

    virtual bool setState(BuzzerState state) override
    {
        NX_DEBUG(this, "%1 the buzzer", (state == BuzzerState::enabled) ? "Enabling" : "Disabling");
        const int command = prepareCommand<int>(kSetBuzzerCommand);
        int commandData = (state == BuzzerState::enabled);

        if (const int result = ::ioctl(m_ioDeviceDescriptor, command, &commandData); result != 0)
        {
            NX_WARNING(this, "Unable to %1 the buzzer, error: %2",
                (state == BuzzerState::enabled ? "enable" : "disable"),
                result);

            return false;
        }

        return true;
    }

private:
    int m_ioDeviceDescriptor = -1;
};

#endif

BuzzerPlatformAbstractionImpl* createPlatformAbstractionImpl(int ioDeviceDescriptor)
{
#if defined(Q_OS_LINUX)
    return new BuzzerPlatformAbstractionImpl(ioDeviceDescriptor);
#else
    return nullptr;
#endif
}


BuzzerPlatformAbstraction::BuzzerPlatformAbstraction(int ioDeviceDescriptor):
    m_impl(createPlatformAbstractionImpl(ioDeviceDescriptor))
{
    NX_DEBUG(this, "Creating the buzzer platform abstraction");
}

BuzzerPlatformAbstraction::~BuzzerPlatformAbstraction()
{
    NX_DEBUG(this, "Destroying th buzzer platform abstraction");
}

bool BuzzerPlatformAbstraction::setState(BuzzerState state)
{
    if (NX_ASSERT(m_impl))
        return m_impl->setState(state);

    return false;
}


} // nx::vms::server::nvr::hanwha
