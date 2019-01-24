#pragma once

#include <memory>

#include <QSet>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>
#include <QDomDocument>

#include <nx/utils/elapsed_timer_thread_safe.h>
#include <nx/utils/system_error.h>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/http_async_client.h>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/i_device_agent.h>

#include "nx/dw_mtt/camera_controller.h"
#include "common.h"
#include "engine.h"
#include "parser.h"
#include <nx/network/system_socket.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_mtt {

/**
 * The purpose of ElapsedEvent is to store information when event of corresponding type happened
 * last time.
 * @note ElapsedEvent is non-copyable and non-movable.
 */
struct ElapsedEvent
{
public:
    const EventType type;
    nx::utils::ElapsedTimerThreadSafe timer;
    ElapsedEvent(const EventType& analyticsEventType): type(analyticsEventType){}
};
using ElapsedEvents = std::list<ElapsedEvent>;

class DeviceAgent: public nxpt::CommonRefCounter<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(Engine* engine,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    void treatAlarmPairs(const QList<AlarmPair>& alarmPairs);

    void sendEventStartedPacket(const EventType& event) const;

    void sendEventStoppedPacket(const EventType& event) const;

    /** When some bytes received from notification server or when connection was broken/closed. */
    void onReceive(SystemError::ErrorCode, size_t);

    virtual nx::sdk::Error setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    QDomDocument createDomFromRequest(const QByteArray& request);

    nx::utils::Url makeUrl(const QString& requestName);
    void prepareHttpClient();
    void makeSubscription();
    void makeDeferredSubscription();
    void onSubsctiptionDone();
    void readNextNotificationAsync();

    QByteArray extractRequestFromBuffer();

private:
    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    nx::utils::Url m_url;
    QAuthenticator m_auth;
    QByteArray m_cameraManifest;
    ElapsedEvents m_eventsToCatch;
    QByteArray m_buffer;
    nx::sdk::analytics::IDeviceAgent::IHandler* m_handler = nullptr;
    nx::network::aio::Timer m_reconnectTimer;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
    nx::dw_mtt::CameraController m_cameraController;
    QSet<QByteArray> internalNamesToCatch() const;

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_tcpSocket;

    QnMutex m_mutex;

    std::atomic<bool> m_terminated{false};
};

} // namespace dw_mtt
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
