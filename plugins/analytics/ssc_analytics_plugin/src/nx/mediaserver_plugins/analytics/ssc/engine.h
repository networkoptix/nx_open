#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtSerialPort/QSerialPort>

#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/analytics/engine.h>
#include <plugins/plugin_tools.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace ssc {

class DeviceAgent;

/** Plugin for working with SSC-camera. */
class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    virtual const char* manifest(nx::sdk::Error* error) const override;

    void registerCamera(int cameraLogicalId, DeviceAgent* deviceAgent);
    void unregisterCamera(int cameraLogicalId);

    virtual void setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/) override {}

    virtual void executeAction(
        nx::sdk::analytics::Action* /*action*/, sdk::Error* /*outError*/) override
    {
    }

private:
    void readAllowedPortNames();
    void initPorts();
    void initPort(const QString& portName, int index);
    void configureSerialPort(QSerialPort* portToTune, const QString& name, int index);
    void onDataReceived(int index);

private:
    QByteArray m_manifest;
    EngineManifest m_typedManifest;
    EventType cameraEventType;
    EventType resetEventType;

    AllowedPortNames m_allowedPortNames;
    QList<QSerialPort*> m_serialPortList;
    QList<QByteArray> m_receivedDataList;

    QMap<int /*cameraLogicalId*/, DeviceAgent*> m_cameraMap;
    QSet<int /*cameraLogicalId*/> m_activeCameras;
    QMutex m_cameraMutex;
};

} // namespace ssc
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
