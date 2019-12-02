#pragma once

#include <api/model/api_ioport_data.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IIoPlatformAbstraction
{
public:
    virtual ~IIoPlatformAbstraction() = default;

    virtual QnIOPortDataList portDescriptors() const = 0;

    virtual bool setOutputPortState(const QString& portId, IoPortState state) = 0;

    virtual QnIOStateData portState(const QString& portId) const = 0;
};

} // namespace nx::vms::server::nvr::hanwha
