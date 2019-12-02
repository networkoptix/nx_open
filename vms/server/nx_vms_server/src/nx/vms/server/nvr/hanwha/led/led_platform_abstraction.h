#pragma once

#include <memory>

#include <nx/vms/server/nvr/hanwha/led/i_led_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

class LedPlatformAbstraction: public ILedPlatformAbstraction
{
public:
    LedPlatformAbstraction(int ioDeviceFileDescriptor);

    virtual ~LedPlatformAbstraction() override;

    virtual std::vector<LedDescription> ledDescriptions() const override;

    virtual bool setLedState(const QString& ledId, LedState state) override;

private:
    std::unique_ptr<ILedPlatformAbstraction> m_impl;
};

} // namespace nx::vms::server::nvr::hanwha
