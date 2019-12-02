#pragma once

#include <memory>

#include <nx/vms/server/nvr/hanwha/buzzer/i_buzzer_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

class BuzzerPlatformAbstractionImpl;

class BuzzerPlatformAbstraction: public IBuzzerPlatformAbstraction
{
public:
    BuzzerPlatformAbstraction(int ioDeviceDescriptor);

    virtual ~BuzzerPlatformAbstraction() override;

    virtual bool setState(BuzzerState state) override;

private:
    std::unique_ptr<BuzzerPlatformAbstractionImpl> m_impl;
};

} // namespace nx::vms::server::nvr::hanwha
