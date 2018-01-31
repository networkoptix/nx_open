#pragma once

#include "hanwha_common.h"
#include "hanwha_metadata_monitor.h"

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <vector>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>


namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaMetadataPlugin:
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataPlugin>
{
public:
    HanwhaMetadataPlugin();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual nx::sdk::metadata::AbstractMetadataManager* managerForResource(
        const nx::sdk::ResourceInfo& resourceInfo,
        nx::sdk::Error* outError) override;

    virtual nx::sdk::metadata::AbstractSerializer* serializerForType(
        const nxpl::NX_GUID& typeGuid,
        nx::sdk::Error* outError) override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) const override;

    const Hanwha::DriverManifest& driverManifest() const;

    HanwhaMetadataMonitor* monitor(
        const QString& sharedId,
        const QUrl& url,
        const QAuthenticator& auth);

    void managerStoppedToUseMonitor(const QString& sharedId);

    void managerIsAboutToBeDestroyed(const QString& sharedId);

private:
    struct SharedResources
    {
        std::unique_ptr<HanwhaMetadataMonitor> monitor;
        std::shared_ptr<nx::mediaserver_core::plugins::HanwhaSharedResourceContext> sharedContext;
        std::atomic<int> monitorUsageCounter{0};
        std::atomic<int> managerCounter{0};

        SharedResources(
            const QString& sharedId,
            const Hanwha::DriverManifest& driverManifest,
            const QUrl& url,
            const QAuthenticator& auth);

        void setResourceAccess(const QUrl& url, const QAuthenticator& auth);
    };

private:
    boost::optional<QList<QnUuid>> fetchSupportedEvents(
        const nx::sdk::ResourceInfo& resourceInfo);

    boost::optional<QList<QnUuid>> eventsFromParameters(
        const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters,
        const nx::mediaserver_core::plugins::HanwhaResponse& eventStatuses,
        int channel);

    std::shared_ptr<SharedResources> sharedResources(
        const nx::sdk::ResourceInfo& resourceInfo);

private:
    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Hanwha::DriverManifest m_driverManifest;
    QMap<QString, std::shared_ptr<SharedResources>> m_sharedResources;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
