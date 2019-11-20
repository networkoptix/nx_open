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
        NX_DEBUG(this, "Creating the buzzer platform abstraction");
    }

    virtual ~BuzzerPlatformAbstractionImpl() override
    {
        NX_DEBUG(this, "Destroying the buzzer platform abstraction");
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

#else

class BuzzerPlatformAbstractionImpl: public IBuzzerPlatformAbstraction
{
public:
    BuzzerPlatformAbstractionImpl(int /*ioDeviceDescriptor*/)
    {
        NX_ASSERT(false);
    }

    virtual ~BuzzerPlatformAbstractionImpl() override
    {
        NX_ASSERT(false);
    }

    virtual bool setState(BuzzerState /*state*/) override
    {
        NX_ASSERT(false);
        return false;
    }
};

#endif

BuzzerPlatformAbstraction::BuzzerPlatformAbstraction(int ioDeviceDescriptor):
    m_impl(std::make_unique<BuzzerPlatformAbstractionImpl>(ioDeviceDescriptor))
{
    NX_DEBUG(this, "Creating the buzzer platform abstraction");
}

BuzzerPlatformAbstraction::~BuzzerPlatformAbstraction()
{
    NX_DEBUG(this, "Destroying th buzzer platform abstraction");
}

bool BuzzerPlatformAbstraction::setState(BuzzerState state)
{
    return m_impl->setState(state);
}


} // nx::vms::server::nvr::hanwha
