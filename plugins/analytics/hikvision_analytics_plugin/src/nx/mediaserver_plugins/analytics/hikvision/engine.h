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
#include <nx/sdk/analytics/engine.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/analytics/common_plugin.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace hikvision {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine(nx::sdk::analytics::CommonPlugin* plugin);

    virtual nx::sdk::analytics::CommonPlugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const char* manifest(
        nx::sdk::Error* error) const override;

    const Hikvision::EngineManifest& engineManifest() const;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

private:
    boost::optional<QList<QString>> fetchSupportedEvents(const nx::sdk::DeviceInfo& deviceInfo);
    QList<QString> parseSupportedEvents(const QByteArray& data);

private:
    nx::sdk::analytics::CommonPlugin* const m_plugin;

    mutable QnMutex m_mutex;
    QByteArray m_manifest;
    Hikvision::EngineManifest m_engineManifest;

    struct DeviceData
    {
        bool hasExpired() const;

        QList<QString> supportedEventTypes;
        nx::utils::ElapsedTimer timeout;
    };

    QMap<QString, DeviceData> m_cachedDeviceData;
};

} // namespace hikvision
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
