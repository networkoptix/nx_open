#pragma once

namespace nx::vms::server::nvr {

class IStartable
{
public:
    virtual ~IStartable() = default;

    virtual void start() = 0;
};

} // namespace nx::vms::server::nvr
