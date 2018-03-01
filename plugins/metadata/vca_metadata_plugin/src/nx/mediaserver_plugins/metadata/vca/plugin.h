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

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

/*
 * Plugin for work with VCA-camera. Deals with three events: motion-detected, vca-event and
 * face-detected.
*/
class Plugin: public nxpt::CommonRefCounter<nx::sdk::metadata::Plugin>
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

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override {}

    // Managers can safely ask plugin about events. If no event found, m_emptyEvent is returned.
    const AnalyticsEventType& eventByInternalName(const QString& internalName) const noexcept;

    const AnalyticsEventType& eventByUuid(const QnUuid& uuid) const noexcept;

    virtual void executeAction(
        nx::sdk::metadata::Action* action, nx::sdk::Error* outError) override;

private:
    QByteArray m_manifest;
    AnalyticsDriverManifest m_typedManifest;
    AnalyticsEventType m_emptyEvent;

};

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
