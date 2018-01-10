#pragma once

#include "common.h"
#include "metadata_monitor.h"

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <vector>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <nx/utils/elapsed_timer.h>
#include <chrono>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

class MetadataPlugin:
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataPlugin>
{
public:
    MetadataPlugin();

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

    const Hikvision::DriverManifest& driverManifest() const;

private:
    boost::optional<QList<QnUuid>> fetchSupportedEvents(
        const nx::sdk::ResourceInfo& resourceInfo);
    QList<QnUuid> parseSupportedEvents(const QByteArray& data);
private:
    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Hikvision::DriverManifest m_driverManifest;

    struct DeviceData
    {
        bool hasExpired() const;

        QList<QnUuid> supportedEventTypes;
        nx::utils::ElapsedTimer timeout;
    };

    QMap<QString, DeviceData> m_cachedDeviceData;
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx

