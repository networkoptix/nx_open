#pragma once

#include <memory>

#include <nx/vms/server/nvr/types.h>
#include <nx/vms/server/nvr/hanwha/io/i_io_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

class IoPlatformAbstraction: public IIoPlatformAbstraction
{
public:
    IoPlatformAbstraction(int ioDeviceDescriptor);

    virtual ~IoPlatformAbstraction() override;

    virtual QnIOPortDataList portDescriptors() const override;

    virtual bool setOutputPortState(const QString& portId, IoPortState state) override;

    virtual QnIOStateData portState(const QString& portId) const override;

private:
    std::unique_ptr<IIoPlatformAbstraction> m_impl;
};

} // namespace nx::vms::server::nvr::hanwha
