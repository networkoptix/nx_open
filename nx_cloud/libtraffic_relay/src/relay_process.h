#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/service.h>
#include <nx/utils/thread/stoppable.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }

class RelayProcess:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    RelayProcess(int argc, char **argv);

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;
};

} // namespace relay
} // namespace cloud
} // namespace nx
