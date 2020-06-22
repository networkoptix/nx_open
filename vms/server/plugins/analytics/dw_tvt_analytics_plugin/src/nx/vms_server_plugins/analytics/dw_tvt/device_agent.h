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

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include "nx/dw_tvt/camera_controller.h"
#include "common.h"
#include "engine.h"
#include "parser.h"
#include <nx/network/system_socket.h>

namespace nx::vms_server_plugins::analytics::dw_tvt {

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

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(Engine* engine,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    void treatAlarmPairs(const QList<AlarmPair>& alarmPairs);

    void sendEventStartedPacket(const EventType& event) const;

    void sendEventStoppedPacket(const EventType& event) const;

    /** When some bytes received from notification server or when connection was broken/closed. */
    void onReceive(SystemError::ErrorCode, size_t);

    virtual void setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    QDomDocument createDomFromRequest(const QByteArray& request);

    nx::utils::Url makeUrl(const QString& requestName);
    void prepareHttpClient(const QByteArray& messageBody,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket = {});
    void makeSubscriptionAsync();
    void makeUnsubscriptionSync(std::unique_ptr<nx::network::AbstractStreamSocket> s);
    void makeDeferredSubscriptionAsync();
    void onSubscriptionDone();
    void readNextNotificationAsync();

    bool logHttpRequestResult();

    QByteArray extractRequestFromBuffer();

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

private:
    nx::sdk::Result<void> startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    nx::utils::Url m_url;
    QAuthenticator m_auth;
    QByteArray m_cameraManifest;
    ElapsedEvents m_eventsToCatch;
    QByteArray m_buffer;
    QByteArray m_unsubscribeBody;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
    nx::network::aio::Timer m_reconnectTimer;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
    nx::dw_tvt::CameraController m_cameraController;
    QSet<QByteArray> internalNamesToCatch() const;

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_tcpSocket;

    QnMutex m_mutex;

    std::atomic<bool> m_terminated{false};
};

} // nx::vms_server_plugins::analytics::dw_tvt
