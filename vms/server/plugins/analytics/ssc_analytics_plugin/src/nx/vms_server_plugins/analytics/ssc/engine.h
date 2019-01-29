#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtSerialPort/QSerialPort>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_engine.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace ssc {

class DeviceAgent;

/** Plugin for working with SSC-camera. */
class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual nx::sdk::analytics::Plugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    void registerCamera(int cameraLogicalId, DeviceAgent* deviceAgent);
    void unregisterCamera(int cameraLogicalId);

    virtual void executeAction(
        nx::sdk::analytics::IAction* /*action*/, sdk::Error* /*outError*/) override;

    virtual nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

private:
    void readAllowedPortNames();
    void initPorts();
    void initPort(const QString& portName, int index);
    void configureSerialPort(QSerialPort* portToTune, const QString& name, int index);
    void onDataReceived(int index);

private:
    nx::sdk::analytics::Plugin* const m_plugin;

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
} // namespace vms_server_plugins
} // namespace nx
