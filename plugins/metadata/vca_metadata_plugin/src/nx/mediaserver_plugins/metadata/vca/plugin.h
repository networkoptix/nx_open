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

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

class Plugin: public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataPlugin>
{
public:
    Plugin();

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

    // Managers can safely ask plugin about events. In no event found m_emptyEvent is returned.
    const Vca::VcaAnalyticsEventType& eventByInternalName(
        const QString& internalName) const noexcept;
private:
    QByteArray m_manifest;
    Vca::VcaAnalyticsDriverManifest m_typedManifest;
    Vca::VcaAnalyticsEventType m_emptyEvent;

};

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
