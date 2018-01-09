#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>
#include <nx/network/socket_global.h>

#include "axis_common.h"
#include "axis_metadata_monitor.h"

namespace nx {
namespace mediaserver {
namespace plugins {

class AxisMetadataPlugin:
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataPlugin>
{
public:
    struct SharedResources
    {
        std::unique_ptr<AxisMetadataMonitor> monitor;
        std::atomic<int> monitorUsageCounter{0};
        std::atomic<int> managerCounter{0};
        QList<QnUuid> supportedEvents;

        SharedResources(
            const QString& sharedId,
            const Axis::DriverManifest& driverManifest,
            const QUrl& url,
            const QAuthenticator& auth);
    };

public:
    AxisMetadataPlugin();

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

    const Axis::DriverManifest& driverManifest() const;

    AxisMetadataMonitor* monitor(
        const QString& sharedId,
        const QUrl& url,
        const QAuthenticator& auth);

    void managerStoppedToUseMonitor(const QString& sharedId);

    void managerIsAboutToBeDestroyed(const QString& sharedId);

    std::shared_ptr<SharedResources> sharedResources(const QString& sharedId);

private:
    QList<AxisEvent> fetchSupportedAxisEvents(
        const nx::sdk::ResourceInfo& resourceInfo);

    boost::optional<QList<QnUuid>> fetchSupportedEvents(
        const nx::sdk::ResourceInfo& resourceInfo);

    std::shared_ptr<SharedResources> sharedResources(
        const nx::sdk::ResourceInfo& resourceInfo);

private:
    nx::network::SocketGlobals::InitGuard m_guard;

    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Axis::DriverManifest m_driverManifest;
    QMap<QString, std::shared_ptr<SharedResources>> m_sharedResources;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
