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

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    const EngineManifest& parsedManifest() const;

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;
    virtual void doExecuteAction(
        nx::sdk::Result<nx::sdk::analytics::IAction::Result>* outResult,
        const nx::sdk::analytics::IAction* action) override;

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
