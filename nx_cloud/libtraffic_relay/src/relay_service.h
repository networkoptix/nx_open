#pragma once

#include <memory>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/service.h>
#include <nx/utils/thread/stoppable.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class View;

class RelayService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    RelayService(int argc, char **argv);

    std::vector<SocketAddress> httpEndpoints() const;

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

private:
    View* m_view = nullptr;
};

} // namespace relay
} // namespace cloud
} // namespace nx
