// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/settings.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_service_settings.h>

namespace nx::network::http::server::test {

class Settings: public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    virtual QString dataDir() const override;
    virtual log::Settings logging() const override;
    const nx::network::http::server::Settings& getHttpSettings() const { return m_httpSettings; };

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    log::Settings m_logging;
    nx::network::http::server::Settings m_httpSettings;

private:
    virtual void loadSettings() override;
};

} // namespace nx::network::http::server::test
