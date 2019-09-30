#pragma once

#include "common.h"
#include "metadata_monitor.h"

#include <vector>
#include <chrono>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/analytics/helpers/plugin.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    const Hikvision::EngineManifest& engineManifest() const;

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::IStringMap*>* outResult,
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
    boost::optional<QList<QString>> fetchSupportedEventTypeIds(
        const nx::sdk::IDeviceInfo* deviceInfo);
    QList<QString> parseSupportedEvents(const QByteArray& data);

private:
    nx::sdk::analytics::Plugin* const m_plugin;

    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Hikvision::EngineManifest m_engineManifest;

    struct DeviceData
    {
        bool hasExpired() const;

        QList<QString> supportedEventTypeIds;
        nx::utils::ElapsedTimer timeout;
    };

    QMap<QString, DeviceData> m_cachedDeviceData;
};

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
