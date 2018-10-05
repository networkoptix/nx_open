#pragma once

#include "common.h"
#include "metadata_monitor.h"

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <vector>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/utils/elapsed_timer.h>
#include <chrono>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace hikvision {

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
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const char* manifest(
        nx::sdk::Error* error) const override;

    const Hikvision::EngineManifest& engineManifest() const;

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

private:
    boost::optional<QList<QString>> fetchSupportedEvents(const nx::sdk::DeviceInfo& deviceInfo);
    QList<QString> parseSupportedEvents(const QByteArray& data);

private:
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
