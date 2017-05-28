#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/service.h>

namespace nx {
namespace time_server {

class TimeServerProcess:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    TimeServerProcess(int argc, char **argv);
    virtual ~TimeServerProcess();

protected:
    virtual std::unique_ptr<nx::utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const nx::utils::AbstractServiceSettings& settings) override;
};

} // namespace time_server
} // namespace nx
