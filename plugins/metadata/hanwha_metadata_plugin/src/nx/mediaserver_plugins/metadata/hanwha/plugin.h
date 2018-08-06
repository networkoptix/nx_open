#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>

#include "common.h"
#include "metadata_monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

class Plugin:
    public nxpt::CommonRefCounter<nx::sdk::metadata::Plugin>
{
public:
    Plugin();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual nx::sdk::metadata::CameraManager* obtainCameraManager(
        const nx::sdk::CameraInfo* cameraInfo,
        nx::sdk::Error* outError) override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) const override;

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

    virtual void executeAction(
        nx::sdk::metadata::Action* action,
        nx::sdk::Error* outError) override;

    const Hanwha::DriverManifest& driverManifest() const;

    MetadataMonitor* monitor(
        const QString& sharedId,
        const nx::utils::Url& url,
        const QAuthenticator& auth);

    void managerStoppedToUseMonitor(const QString& sharedId);

    void managerIsAboutToBeDestroyed(const QString& sharedId);

private:
    struct SharedResources
    {
        std::unique_ptr<MetadataMonitor> monitor;
        std::shared_ptr<nx::mediaserver_core::plugins::HanwhaSharedResourceContext> sharedContext;
        std::atomic<int> monitorUsageCounter{0};
        std::atomic<int> managerCounter{0};

        SharedResources(
            const QString& sharedId,
            const Hanwha::DriverManifest& driverManifest,
            const nx::utils::Url &url,
            const QAuthenticator& auth);

        void setResourceAccess(const nx::utils::Url& url, const QAuthenticator& auth);
    };

private:
    boost::optional<QList<QnUuid>> fetchSupportedEvents(
        const nx::sdk::CameraInfo& cameraInfo);

    boost::optional<QList<QnUuid>> eventsFromParameters(
        const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters,
        const nx::mediaserver_core::plugins::HanwhaResponse& eventStatuses,
        int channel) const;

    std::shared_ptr<SharedResources> sharedResources(
        const nx::sdk::CameraInfo& cameraInfo);

private:
    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Hanwha::DriverManifest m_driverManifest;
    QMap<QString, std::shared_ptr<SharedResources>> m_sharedResources;
};

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

