#pragma once

#include <nx/utils/log/log_settings.h>
#include <nx/utils/basic_service_settings.h>

namespace nx {
namespace time_server {
namespace conf {

class Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    virtual QString dataDir() const override;
    virtual nx::utils::log::Settings logging() const override;

protected:
    virtual void loadSettings() override;

private:
    QString m_dataDir;
    utils::log::Settings m_logging;
};

} // namespace conf
} // namespace time_server
} // namespace nx
