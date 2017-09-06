#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <vector>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include "hanwha_common.h"

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

private:
    boost::optional<std::vector<QnUuid>> fetchSupportedEvents(
        const QUrl& url,
        const QAuthenticator& auth);

    boost::optional<std::vector<QnUuid>> eventsFromParameters(
        const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters);

    QUrl buildAttributesUrl(const QUrl& resourceUrl) const;
    QByteArray buildDeviceManifest(const std::vector<QnUuid>& supportedEvents) const;

private:
    QByteArray m_manifest;
    Hanwha::DriverManifest m_driverManifest;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
