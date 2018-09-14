#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtSerialPort/QSerialPort>

#include <nx/sdk/metadata/camera_manager.h>
#include <nx/sdk/metadata/plugin.h>
#include <plugins/plugin_tools.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

class Manager;

/** Plugin for working with SSC-camera. */
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
        const nx::sdk::CameraInfo* cameraInfo, nx::sdk::Error* outError) override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

    void registerCamera(int cameraLogicalId, Manager* manager);
    void unregisterCamera(int cameraLogicalId);

    virtual void setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/) override {}

    virtual void executeAction(
        sdk::metadata::Action* /*action*/, sdk::Error* /*outError*/) override
    {
    }

private:
    void onDataReceived(int index);
    void configureSerialPort(QSerialPort* portToTune, const QString& name, int index);

private:
    QByteArray m_manifest;
    AnalyticsDriverManifest m_typedManifest;
    AnalyticsEventType cameraEventType;
    AnalyticsEventType resetEventType;

    QList<QSerialPort*> m_serialPortList;
    QList<QByteArray> m_receivedDataList;

    QMap<int /*camera logical id*/, Manager*> m_cameraMap;
    QSet<int /*camera logical id*/> m_activeCameras;
    QMutex m_cameraMutex;
};

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
