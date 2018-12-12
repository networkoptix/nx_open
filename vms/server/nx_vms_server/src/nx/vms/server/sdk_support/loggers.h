#pragma once

#include <QtCore/QString>

#include <nx/sdk/analytics/i_plugin.h>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/utils/log/log.h>

namespace nx::vms::server::sdk_support {

class AbstractManifestLogger
{
public:
    virtual ~AbstractManifestLogger() = default;

    virtual void log(
        const QString& manifestStr,
        nx::sdk::Error error,
        const QString& customError = QString()) = 0;
};

/**
 * Supports placeholders {:plugin}, {:engine}, {:device}, {:error} in message template.
 */
class ManifestLogger: public AbstractManifestLogger
{
public:
    ManifestLogger(
        nx::utils::log::Tag logTag,
        QString messageTemplate,
        resource::AnalyticsPluginResourcePtr pluginResource);

    ManifestLogger(
        nx::utils::log::Tag logTag,
        QString messageTemplate,
        resource::AnalyticsEngineResourcePtr engineResource);

    ManifestLogger(
        nx::utils::log::Tag logTag,
        QString messageTemplate,
        QnVirtualCameraResourcePtr device,
        resource::AnalyticsEngineResourcePtr engineResource);

    virtual void log(
        const QString& manifestStr,
        nx::sdk::Error error,
        const QString& customError) override;

private:
    QString buildLogString(const QString& error) const;

private:
    nx::utils::log::Tag m_logTag;
    QString m_messageTemplate;
    QnVirtualCameraResourcePtr m_device;
    resource::AnalyticsPluginResourcePtr m_pluginResource;
    resource::AnalyticsEngineResourcePtr m_engineResource;
};

/**
 * Used on server startup when resources are not available yet.
 */
class StartupPluginManifestLogger: public AbstractManifestLogger
{
public:
    StartupPluginManifestLogger(
        nx::utils::log::Tag logTag,
        const nx::sdk::analytics::IPlugin* plugin);

    virtual void log(
        const QString& manifestStr,
        nx::sdk::Error error,
        const QString& customError) override;

private:
    QString buildLogString(const QString& error) const;

private:
    nx::utils::log::Tag m_logTag;
    const nx::sdk::analytics::IPlugin* m_plugin = nullptr;
};

} // namespace nx::vms::server::sdk_support
