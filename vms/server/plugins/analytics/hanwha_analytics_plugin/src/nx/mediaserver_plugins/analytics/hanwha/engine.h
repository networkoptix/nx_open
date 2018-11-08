#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/common_plugin.h>
#include <nx/sdk/analytics/engine.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>

#include "common.h"
#include "metadata_monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace hanwha {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine(nx::sdk::analytics::CommonPlugin* plugin);

    virtual nx::sdk::analytics::CommonPlugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const char* manifest(
        nx::sdk::Error* error) const override;

    virtual void executeAction(
        nx::sdk::analytics::Action* action,
        nx::sdk::Error* outError) override;

    const Hanwha::EngineManifest& engineManifest() const;

    MetadataMonitor* monitor(
        const QString& sharedId,
        const nx::utils::Url& url,
        const QAuthenticator& auth);

    void deviceAgentStoppedToUseMonitor(const QString& sharedId);

    void deviceAgentIsAboutToBeDestroyed(const QString& sharedId);

private:
    struct SharedResources
    {
        std::unique_ptr<MetadataMonitor> monitor;
        std::shared_ptr<nx::mediaserver_core::plugins::HanwhaSharedResourceContext> sharedContext;
        std::atomic<int> monitorUsageCount{0};
        std::atomic<int> deviceAgentCount{0};

        SharedResources(
            const QString& sharedId,
            const Hanwha::EngineManifest& driverManifest,
            const nx::utils::Url &url,
            const QAuthenticator& auth);

        void setResourceAccess(const nx::utils::Url& url, const QAuthenticator& auth);
    };

private:
    boost::optional<QList<QString>> fetchSupportedEventTypeIds(
        const nx::sdk::DeviceInfo& deviceInfo);

    boost::optional<QList<QString>> eventTypeIdsFromParameters(
        const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters,
        const nx::mediaserver_core::plugins::HanwhaResponse& eventStatuses,
        int channel) const;

    std::shared_ptr<SharedResources> sharedResources(
        const nx::sdk::DeviceInfo& deviceInfo);

private:
    nx::sdk::analytics::CommonPlugin* const m_plugin;

    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Hanwha::EngineManifest m_engineManifest;
    QMap<QString, std::shared_ptr<SharedResources>> m_sharedResources;
};

} // namespace hanwha
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

