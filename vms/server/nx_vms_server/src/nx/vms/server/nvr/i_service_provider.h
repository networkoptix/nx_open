#pragma once

#include <memory>

namespace nx::vms::server::nvr {

class IService;

class IServiceProvider
{
public:
    virtual ~IServiceProvider() = default;

    virtual std::unique_ptr<IService> createService() const = 0;
};

} // namespace nx::vms::server:nvr
