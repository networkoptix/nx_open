#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/network/socket_global.h>

#include "identified_supported_event.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

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
        const nx::sdk::CameraInfo& cameraInfo,
        nx::sdk::Error* outError) override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) const override;

private:
    QList<IdentifiedSupportedEvent> fetchSupportedEvents(
        const nx::sdk::CameraInfo& cameraInfo);

private:
    QByteArray m_manifest;
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
