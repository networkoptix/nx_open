#pragma once

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/basic_service_settings.h>

namespace nx::cloud::storage::service {

class Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

protected:
    virtual void loadSettings() override;
};

} // namespace nx::cloud::storage::service
