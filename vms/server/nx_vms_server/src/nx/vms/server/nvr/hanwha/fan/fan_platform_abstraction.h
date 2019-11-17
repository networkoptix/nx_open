#pragma once

#include <memory>

#include <nx/vms/server/nvr/hanwha/fan/i_fan_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

class FanPlatformAbstractionImpl;

class FanPlatformAbstraction: public IFanPlatformAbstraction
{
public:
    FanPlatformAbstraction(int ioDeviceDescriptor);

    virtual ~FanPlatformAbstraction() override;

    virtual FanState state() const override;

    virtual void interrupt() override;

private:
    std::unique_ptr<FanPlatformAbstractionImpl> m_impl;
};

} // namespace nx::vms::server::nvr::hanwha
