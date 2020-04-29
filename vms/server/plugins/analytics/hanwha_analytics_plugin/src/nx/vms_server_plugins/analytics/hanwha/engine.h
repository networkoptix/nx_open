#pragma once

#include <optional>

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_engine.h>

#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>

#include "common.h"
#include "metadata_monitor.h"

namespace nx::vms_server_plugins::analytics::hanwha {

class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
private:
    struct SharedResources
    {
        std::unique_ptr<MetadataMonitor> monitor;
        std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext> sharedContext;
        std::atomic<int> monitorUsageCount{ 0 };
        std::atomic<int> deviceAgentCount{ 0 };

        SharedResources(
            const QString& sharedId,
            const Hanwha::EngineManifest& driverManifest,
            const nx::utils::Url &url,
            const QAuthenticator& auth);

        void setResourceAccess(const nx::utils::Url& url, const QAuthenticator& auth);
    };

public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    const Hanwha::EngineManifest& manifest() const;
    const Hanwha::ObjectMetadataAttributeFilters& objectMetadataAttributeFilters() const;

    MetadataMonitor* monitor(
        const QString& sharedId,
        const nx::utils::Url& url,
        const QAuthenticator& auth);

    void deviceAgentStoppedToUseMonitor(const QString& sharedId);

    void deviceAgentIsAboutToBeDestroyed(const QString& sharedId);

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

    std::shared_ptr<vms::server::plugins::HanwhaSharedResourceContext> sharedContext(
        QString m_sharedId);

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::IStringMap*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;
    virtual void doExecuteAction(
        nx::sdk::Result<nx::sdk::analytics::IAction::Result>* outResult,
        const nx::sdk::analytics::IAction* action) override;

private:
    static bool fetchSupportsObjectDetection(
        const std::shared_ptr<SharedResources>& sharedRes,
        int channel);

    static QSize fetchMaxResolution(
        const std::shared_ptr<SharedResources>& sharedRes,
        const nx::sdk::IDeviceInfo* deviceInfo);

    static bool fetchIsNvr(const std::shared_ptr<SharedResources>& sharedRes);

    std::optional<QSet<QString>> fetchSupportedEventTypeIds(
        const std::shared_ptr<SharedResources>& sharedRes,
        const nx::sdk::IDeviceInfo* deviceInfo,
        std::optional<QSet<QString>> filter) const;

    std::optional<Hanwha::DeviceAgentManifest> buildDeviceAgentManifest(
        const std::shared_ptr<SharedResources>& sharedRes,
        const nx::sdk::IDeviceInfo* deviceInfo,
        bool areSettingsAndTrackingAllowed,
        std::optional<QSet<QString>> filter) const;

    std::optional<QSet<QString>> eventTypeIdsFromParameters(
        const nx::utils::Url& url,
        const nx::vms::server::plugins::HanwhaCgiParameters& parameters,
        const nx::vms::server::plugins::HanwhaResponse& eventStatuses,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const std::optional<QSet<QString>>& filter) const;

    std::shared_ptr<SharedResources> sharedResources(const nx::sdk::IDeviceInfo* deviceInfo);

private:
    nx::sdk::analytics::Plugin* const m_plugin;

    mutable QnMutex m_mutex;
    Hanwha::EngineManifest m_manifest;
    Hanwha::ObjectMetadataAttributeFilters m_objectMetadataAttributeFilters;

    QMap<QString, std::shared_ptr<SharedResources>> m_sharedResources;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
