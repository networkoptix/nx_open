#pragma once

#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

/** Plugin for work with DWMTT-camera. */
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

    const AnalyticsEventType* eventByUuid(const QnUuid& uuid) const noexcept;
 //   const AnalyticsDriverManifest& typedManifest() const noexcept { return m_typedManifest; }

private:
    QByteArray m_manifest;
    AnalyticsDriverManifest m_typedManifest;
};

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
