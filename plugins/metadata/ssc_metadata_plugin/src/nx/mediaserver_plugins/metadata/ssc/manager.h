#pragma once

#include <memory>

#include <QtCore/QUrl>
#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>

#include <nx/sdk/metadata/camera_manager.h>

#include "common.h"
#include "plugin.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

class Plugin;

class Manager : public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
public:
    Manager(
        Plugin* plugin,
        const nx::sdk::CameraInfo& cameraInfo,
        const AnalyticsDriverManifest& typedManifest);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    void sendEventPacket(const AnalyticsEventType& event) const;

    virtual nx::sdk::Error startFetchingMetadata(
        const char* const* tpeList, int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual sdk::Error setHandler(sdk::metadata::MetadataHandler* handler) override;

    virtual void setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/) override {}

private:
    Plugin * const m_plugin;
    const QUrl m_url;
    const int m_cameraLogicalId;
    QByteArray m_cameraManifest;
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
};

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
