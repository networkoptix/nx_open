#pragma once

#include <map>
#include <vector>

#include <nx/vms/server/nvr/i_startable.h>

namespace nx::vms::server::nvr {

class ILedController: public IStartable
{

public:
    using LedId = QString;

    enum class LedState
    {
        enabled,
        disabled,
    };

    struct LedDescriptor
    {
        LedId id;
        QString name;
    };

public:
    virtual ~ILedController() = default;

    virtual void start() override = 0;

    virtual std::vector<LedDescriptor> ledDescriptors() const = 0;

    virtual std::map<LedId, LedState> ledStates() const = 0;

    virtual bool setState(const QString& ledId, LedState state) = 0;
};

} // namespace nx::vms::server::nvr
