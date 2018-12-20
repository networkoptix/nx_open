#pragma once

#include "common.h"
#include "metadata_monitor.h"

#include <vector>
#include <chrono>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/analytics/common/plugin.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::common::Plugin* plugin);

    virtual nx::sdk::analytics::common::Plugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    const Hikvision::EngineManifest& engineManifest() const;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

    virtual nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::DeviceInfo* deviceInfo) const override;

private:
    boost::optional<QList<QString>> fetchSupportedEventTypeIds(const nx::sdk::DeviceInfo& deviceInfo);
    QList<QString> parseSupportedEvents(const QByteArray& data);

private:
    nx::sdk::analytics::common::Plugin* const m_plugin;

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
