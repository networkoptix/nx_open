// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/ssl/context.h>
#include <nx/utils/file_watcher.h>

#include "settings.h"

namespace nx::network::http::server {

/**
 * SSL-related resources of an HTTPS server.
 */
class NX_NETWORK_API HttpsServerContext
{
public:
    /**
     * Loads settings and configures SSL context and certificate appropriately.
     *
     * NOTE: Throws on error.
     */
    HttpsServerContext(const Settings& settings);

    ssl::Context& context();

private:
    void loadCertificate(bool isMonitoringEnabled);
    void initializeCertificateFileMonitoring();

private:
    Settings::Ssl m_settings;
    std::unique_ptr<ssl::Context> m_sslContext;
    nx::utils::SubscriptionId m_subscriptionId = nx::utils::kInvalidSubscriptionId;
    std::unique_ptr<nx::utils::file_system::FileWatcher> m_fileWatcher;
};

} // namespace nx::network::http::server
