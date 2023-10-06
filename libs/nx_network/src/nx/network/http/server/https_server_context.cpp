// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "https_server_context.h"

#include <QtCore/QFile>

#include <nx/utils/log/log.h>

namespace nx::network::http::server {

HttpsServerContext::HttpsServerContext(const Settings& settings):
    m_sslContext(std::make_unique<ssl::Context>())
{
    if (!settings.ssl.allowedSslVersions.empty())
    {
        if (!m_sslContext->setAllowedServerVersions(settings.ssl.allowedSslVersions))
        {
            const auto error = nx::format("Failed to apply SSL versions %1")
                .args(settings.ssl.allowedSslVersions).toStdString();
            NX_INFO(this, error);
            throw std::runtime_error(error);
        }
    }

    if (!settings.ssl.certificatePath.empty())
    {
        m_certificatePath = settings.ssl.certificatePath;

        const bool isMonitoringEnabled = settings.ssl.certificateMonitorTimeout.has_value();
        loadCertificate(isMonitoringEnabled);
        if (isMonitoringEnabled)
            initializeCertificateFileMonitoring(settings);
    }
}

ssl::Context& HttpsServerContext::context()
{
    return *m_sslContext;
}

void HttpsServerContext::loadCertificate(bool isMonitoringEnabled)
{
    QFile file(QString::fromStdString(m_certificatePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        const auto error = nx::format("Failed to open certificate file '%1': %2")
            .args(m_certificatePath, file.errorString()).toStdString();
        NX_INFO(this, error);
        if (isMonitoringEnabled)
            return; // Certificate may appear later.
        throw std::runtime_error(error);
    }
    auto certData = file.readAll().toStdString();

    if (!m_sslContext->setDefaultCertificate(certData))
    {
        const auto error = nx::format("Failed to load certificate from '%1'")
            .args(m_certificatePath).toStdString();
        NX_INFO(this, error);
        throw std::runtime_error(error);
    }
}

void HttpsServerContext::initializeCertificateFileMonitoring(const Settings& settings)
{
    m_fileWatcher = std::make_unique<nx::utils::file_system::FileWatcher>(
        *settings.ssl.certificateMonitorTimeout);

    const auto systemError = m_fileWatcher->subscribe(
        m_certificatePath,
        [this](
            const auto& /*filePath*/,
            SystemError::ErrorCode resultCode,
            auto /*watchEvent*/)
        {
            if (resultCode != SystemError::noError)
            {
                NX_WARNING(this, "Error %1 occurred while watching SSL certificate file %2",
                    SystemError::toString(resultCode), m_certificatePath);
                return;
            }

            NX_INFO(this, "SSL certificate file %1 changed. Reloading...", m_certificatePath);
            try
            {
                loadCertificate(true);
            }
            catch (const std::runtime_error& e)
            {
                NX_WARNING(this, "Failed to reload certificate %1. %2", m_certificatePath, e.what());
            }
        },
        &m_subscriptionId,
        nx::utils::file_system::FileWatcher::metadata | nx::utils::file_system::FileWatcher::hash);

    if (systemError)
    {
        NX_WARNING(this, "Failed to watch ssl certificate file for changes: %1",
            SystemError::toString(systemError));
    }
}

} // namespace nx::network::http::server
