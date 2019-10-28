#pragma once

namespace nx::vms::server::nvr {

class ILedController
{

public:
    enum class LedState
    {
        enabled,
        disabled,
    };

    struct LedDescriptor
    {
        QString id;
        QString name;
    };

public:
    virtual ~ILedController() = default;

    virtual std::vector<LedDescriptor> ledDescriptors() const = 0;

    virtual std::vector<LedState> ledStates() const = 0;

    virtual bool enable(const QString& ledId) = 0;

    virtual bool disable(const QString& ledId) = 0;

};

} // namespace nx::vms::server::nvr
