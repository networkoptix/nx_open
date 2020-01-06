#pragma once

#include <memory>
#include <vector>

#include <nx/utils/service.h>

#include "settings.h"

namespace nx::cloud::db {

class Controller;
class HttpView;

namespace conf { class Settings; }

class CloudDbService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    CloudDbService(int argc, char **argv);

    std::vector<network::SocketAddress> httpEndpoints() const;

    Controller& controller();

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

private:
    const conf::Settings* m_settings;
    HttpView* m_view = nullptr;
    Controller* m_controller = nullptr;
};

} // namespace nx::cloud::db
