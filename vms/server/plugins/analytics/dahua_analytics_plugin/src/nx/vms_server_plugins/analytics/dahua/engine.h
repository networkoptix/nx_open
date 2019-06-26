#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/analytics/helpers/plugin.h>

#include "common.h"
#include "metadata_monitor.h"

namespace nx::vms_server_plugins::analytics::dahua {

class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual nx::sdk::analytics::Plugin* plugin() const override { return m_plugin; }

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    virtual void setSettings(
        const nx::sdk::IStringMap* settings,
        nx::sdk::IError* outError) override;

    virtual nx::sdk::IStringMap* pluginSideSettings(nx::sdk::IError* outError) const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo,
        nx::sdk::IError* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::IError* outError) const override;

    const EngineManifest& parsedManifest() const;

    virtual void executeAction(
        nx::sdk::analytics::IAction* action, nx::sdk::IError* outError) override;

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

private:
    nx::vms::api::analytics::DeviceAgentManifest fetchDeviceAgentParsedManifest(
        const nx::sdk::IDeviceInfo* deviceInfo);
    QList<QString> parseSupportedEvents(const QByteArray& data);

    static QByteArray loadManifest();
    static EngineManifest parseManifest(const QByteArray& manifest);
private:
    nx::sdk::analytics::Plugin* const m_plugin;

    mutable QnMutex m_mutex;

    // Engine manifest is stored in serialized and deserialized states, since both of them needed.
    const QByteArray m_jsonManifest;
    const EngineManifest m_parsedManifest;

    struct DeviceData
    {
        bool hasExpired() const;

        QList<QString> supportedEventTypeIds;
        nx::utils::ElapsedTimer timeout;
    };

    QMap<QString, DeviceData> m_cachedDeviceData;
};

} // namespace nx::vms_server_plugins::analytics::dahua
