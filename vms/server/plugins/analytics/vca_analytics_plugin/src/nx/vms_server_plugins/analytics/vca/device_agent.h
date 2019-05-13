#pragma once

#include <chrono>

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/elapsed_timer_thread_safe.h>
#include <nx/utils/system_error.h>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_global.h>

#include <nx/sdk/helpers/ref_countable.h>

#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include <nx/network/aio/timer.h>
#include <nx/network/system_socket.h>

#include "common.h"
#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace vca {

/**
 * The purpose of ElapsedEvent is to store information when event of corresponding type happened
 * last time.
 */
struct ElapsedEvent
{
public:
    const EventType type;
    nx::utils::ElapsedTimerThreadSafe timer;
    ElapsedEvent(const EventType& analyticsEventType): type(analyticsEventType){}
};
using ElapsedEvents = std::list<ElapsedEvent>;

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(
        Engine* engine,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    void treatMessage(int size);

    void onReceive(SystemError::ErrorCode, size_t);

    void onConnect(SystemError::ErrorCode code);

    void reconnectSocket();

    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    nx::sdk::Error stopFetchingMetadata();

    virtual nx::sdk::Error setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    bool isTimerNeeded() const;

    std::chrono::milliseconds timeTillCheck() const;

    void sendEventStartedPacket(const EventType& event) const;

    void sendEventStoppedPacket(const EventType& event) const;

    void onTimer();

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

private:
    Engine* const m_engine;

    QUrl m_url;
    QAuthenticator m_auth;
    QByteArray m_cameraManifest;
    ElapsedEvents m_eventsToCatch;
    QByteArray m_buffer;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;

    std::unique_ptr<nx::network::TCPSocket> m_tcpSocket;
    nx::network::aio::Timer m_stopEventTimer;
    nx::network::aio::Timer m_reconnectTimer;
    network::SocketAddress m_cameraAddress;
};

} // namespace vca
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
