#pragma once

#include <chrono>

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/elapsed_timer_thread_safe.h>
#include <nx/utils/system_error.h>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_global.h>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/device_agent.h>

#include <nx/network/aio/timer.h>
#include <nx/network/system_socket.h>

#include "common.h"
#include "engine.h"

namespace nx {
namespace mediaserver_plugins {
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

    void treatMessage(int size);

    void onReceive(SystemError::ErrorCode, size_t);

    void onConnect(SystemError::ErrorCode code);

    void reconnectSocket();

    virtual nx::sdk::Error startFetchingMetadata(
        const char* const* typeList, int typeListSize) override;

    virtual nx::sdk::Error setMetadataHandler(nx::sdk::analytics::MetadataHandler* metadataHandler) override;

    bool isTimerNeeded() const;

    std::chrono::milliseconds timeTillCheck() const;

    void sendEventStartedPacket(const EventType& event) const;

    void sendEventStoppedPacket(const EventType& event) const;

    void onTimer();

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* manifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual void setSettings(const nx::sdk::Settings* settings) override;

    virtual nx::sdk::Settings* settings() const override;

private:
    Engine* const m_engine;

    QUrl m_url;
    QAuthenticator m_auth;
    QByteArray m_cameraManifest;
    ElapsedEvents m_eventsToCatch;
    QByteArray m_buffer;
    nx::sdk::analytics::MetadataHandler* m_metadataHandler = nullptr;

    std::unique_ptr<nx::network::TCPSocket> m_tcpSocket;
    nx::network::aio::Timer m_stopEventTimer;
    nx::network::aio::Timer m_reconnectTimer;
    network::SocketAddress m_cameraAddress;
};

} // namespace vca
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
