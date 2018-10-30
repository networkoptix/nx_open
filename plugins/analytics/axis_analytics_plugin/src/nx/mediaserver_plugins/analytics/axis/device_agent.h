#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/device_agent.h>

#include "common.h"
#include "monitor.h"
#include "engine.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace axis {

class MetadataHandler;

class DeviceAgent: public nxpt::CommonRefCounter<nx::sdk::analytics::DeviceAgent>
{
public:
    DeviceAgent(
        Engine* engine,
        const nx::sdk::DeviceInfo& deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setMetadataHandler(
        nx::sdk::analytics::MetadataHandler* metadataHandler) override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const char* manifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    const EngineManifest& events() const noexcept
    {
        return m_typedManifest;
    }

    virtual void setSettings(const nx::sdk::Settings* settings) override;

    virtual nx::sdk::Settings* settings() const override;

    /** @return Null if not found. */
    const EventType* eventTypeById(const QString& id) const noexcept;

private:
    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    EngineManifest m_typedManifest;
    QByteArray m_manifest;
    QUrl m_url;
    QAuthenticator m_auth;
    Monitor* m_monitor = nullptr;

    /**
     * Place to store manifests we gave to the caller to provide 1) sufficient lifetime and
     * 2) memory releasing in destructor.
     */
    QList<QByteArray> m_givenManifests;

    nx::sdk::analytics::MetadataHandler* m_metadataHandler = nullptr;
};

} // namespace axis
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
