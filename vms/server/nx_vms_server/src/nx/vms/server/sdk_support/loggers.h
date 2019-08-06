#pragma once

#include <QtCore/QString>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_plugin.h>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/sdk_support/error.h>

#include <nx/utils/log/log.h>

class PluginManager;

namespace nx::vms::server::sdk_support {

class AbstractManifestLogger
{
public:
    virtual ~AbstractManifestLogger() = default;

    virtual void log(
        const QString& manifestStr,
        const sdk_support::Error& error) = 0;
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
        const sdk_support::Error& error) override;

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
        const nx::sdk::analytics::IPlugin* plugin,
        const PluginManager* pluginManager);

    virtual void log(
        const QString& manifestStr,
        const sdk_support::Error& error) override;

private:
    nx::utils::log::Tag m_logTag;
    const nx::sdk::analytics::IPlugin* m_plugin = nullptr;
    const PluginManager* const m_pluginManager;
};

} // namespace nx::vms::server::sdk_support
