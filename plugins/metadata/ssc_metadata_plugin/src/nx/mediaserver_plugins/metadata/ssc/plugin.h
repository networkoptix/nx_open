#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QMutex>

#include <QtSerialPort/QSerialPort>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>

#include "manager.h"
#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

class Manager;

/** Plugin for work with SSC-camera. */
class Plugin: public nxpt::CommonRefCounter<nx::sdk::metadata::Plugin>, public QObject
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

    void registerCamera(int cameraLogicalId, Manager* manager);
    void unregisterCamera(int cameraLogicalId);

private:
    void onDataReceived();
    void tuneSerialPort();

private:
    QByteArray m_manifest;
    AnalyticsDriverManifest m_typedManifest;
    AnalyticsEventType cameraEventType;
    AnalyticsEventType resetEventType;

    QSerialPort m_serialPort;
    QByteArray m_receivedData;

    QMap<int /*camera logical id*/, Manager*> m_cameraMap;
    QSet<int /*camera logical id*/> m_activeCameras;
    QMutex m_cameraMutex;
};

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
