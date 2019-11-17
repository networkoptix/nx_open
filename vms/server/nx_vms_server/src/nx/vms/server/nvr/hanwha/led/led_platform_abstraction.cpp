#include "led_platform_abstraction.h"

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/common.h>

#if defined (Q_OS_LINUX)
    #include <nx/vms/server/nvr/hanwha/ioctl_common.h>
#endif

namespace nx::vms::server::nvr::hanwha {

#if defined (Q_OS_LINUX)

static const std::map<QString, int> kCommandByLedId = {
    {kRecordingLedId, kSetRecordingLedCommand},
    {kPoeOverBudgetLedId, kSetPoeOverBudgetLedCommand},
    {kAlarmOutputLedId, kSetAlarmOutputLedCommand}
};

class LedPlatformAbstractionImpl: public ILedPlatformAbstraction
{
public:
    LedPlatformAbstractionImpl(int ioDeviceFileDescriptor):
        m_ioDeviceFileDescriptor(ioDeviceFileDescriptor)
    {
        NX_DEBUG(this, "Creating LED platform abstraction");
    }

    virtual ~LedPlatformAbstractionImpl() override
    {
        // TODO: #dmsihin close descriptor?
        NX_DEBUG(this, "Destroying LED platform abstraction");
    }

    virtual std::vector<LedDescription> ledDescriptions() const override
    {
        NX_DEBUG(this, "Request LED descriptor");
        return {
            {kRecordingLedId, "Recording LED"},
            {kPoeOverBudgetLedId, "PoE over budget LED"},
            {kAlarmOutputLedId, "Alarm Output LED"}
        };
    }

    virtual bool setLedState(const QString& ledId, LedState state) override
    {
        NX_DEBUG(this, "%1 LED %2",
            (state == LedState::enabled ? "Enabling" : "Disabling"),
            ledId);

        if (const auto it = kCommandByLedId.find(ledId); it != kCommandByLedId.cend())
        {
            const int command = prepareCommand<int>(it->second);
            int commandData = (state == LedState::enabled) ? 1 : 0;

            if (const int result = ::ioctl(m_ioDeviceFileDescriptor, command, &commandData);
                result != 0)
            {
                NX_DEBUG(this, "Unable to set state of LED %1 to %2",
                    ledId,
                    ((state == LedState::enabled) ? "'enabled'" : "'disabled'"));

                return false;
            }

            return true;
        }

        NX_DEBUG(this, "Unknown LED id: %1", ledId);
        return false;

    }

private:
    int m_ioDeviceFileDescriptor = -1;
};

#else
class LedPlatformAbstractionImpl
{
public:
    virtual LedPlatformAbstraction(int /*ioDeviceFileDescriptor*/) override
    {
        NX_ASSERT(false);
    }

    virtual ~LedPlatformAbstraction() override
    {
        NX_ASSERT(false);
    }

    virtual std::vector<LedDescription> ledDescriptions() const override
    {
        NX_ASSERT(false);
        return {};
    }

    virtual bool setLedState(const QString& /*ledId*/, LedState /*state*/) override
    {
        NX_ASSERT(false);
        return false;
    }
};

#endif



LedPlatformAbstraction::LedPlatformAbstraction(int ioDeviceFileDescriptor):
    m_impl(std::make_unique<LedPlatformAbstractionImpl>(ioDeviceFileDescriptor))
{
}

LedPlatformAbstraction::~LedPlatformAbstraction()
{
    // TODO: #dmishin implement
}

std::vector<LedDescription> LedPlatformAbstraction::ledDescriptions() const
{
    return m_impl->ledDescriptions();
}

bool LedPlatformAbstraction::setLedState(const QString& ledId, LedState state)
{
    return m_impl->setLedState(ledId, state);
}


} // namespace nx::vms::server::nvr::hanwha
