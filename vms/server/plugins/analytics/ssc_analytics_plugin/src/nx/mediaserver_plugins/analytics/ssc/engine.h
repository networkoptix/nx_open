#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtSerialPort/QSerialPort>

#include <nx/sdk/analytics/common_plugin.h>
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
    Engine(nx::sdk::analytics::CommonPlugin* plugin);

    virtual nx::sdk::analytics::CommonPlugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::Settings* settings) override;

    virtual nx::sdk::Settings* settings() const override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    void registerCamera(int cameraLogicalId, DeviceAgent* deviceAgent);
    void unregisterCamera(int cameraLogicalId);

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
    nx::sdk::analytics::CommonPlugin* const m_plugin;

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
