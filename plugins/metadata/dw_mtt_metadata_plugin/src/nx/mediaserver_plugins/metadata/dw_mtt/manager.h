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

#include <nx/sdk/metadata/camera_manager.h>

#include "nx/dw_mtt/camera_controller.h"
#include "common.h"
#include "plugin.h"
#include "parser.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

/**
 * The purpose of ElapsedEvent is to store information when event of corresponding type happened
 * last time.
 * @note ElapsedEvent is non-copyable and non-movable.
 */
struct ElapsedEvent
{
public:
    const AnalyticsEventType type;
    nx::utils::ElapsedTimerThreadSafe timer;
    ElapsedEvent(const AnalyticsEventType& analyticsEventType): type(analyticsEventType){}
};
using ElapsedEvents = std::list<ElapsedEvent>;

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
public:
    Manager(Plugin* plugin,
        const nx::sdk::CameraInfo& cameraInfo,
        const AnalyticsDriverManifest& typedManifest);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    void treatAlarmPairs(const QList<AlarmPair>& alarmPairs);

    void sendEventStartedPacket(const AnalyticsEventType& event) const;

    void sendEventStoppedPacket(const AnalyticsEventType& event) const;

    /** When some bytes received from notification server or when connection was broken/closed. */
    void onReceive(SystemError::ErrorCode, size_t);

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::MetadataHandler* handler,
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    QDomDocument createDomFromRequest(const QByteArray& request);

    QUrl makeUrl(const QString& requestName);
    void prepareHttpClient();
    void makeSubscription();
    void makeDeferredSubscription();
    void onSubsctiptionDone();
    void readNextNotificationAsync();

    QByteArray extractRequestFromBuffer();

private:
    QUrl m_url;
    QAuthenticator m_auth;
    Plugin* m_plugin;
    QByteArray m_cameraManifest;
    ElapsedEvents m_eventsToCatch;
    QByteArray m_buffer;
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;
    nx::network::aio::Timer m_reconnectTimer;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
    nx::dw_mtt::CameraController m_cameraController;
    QSet<QByteArray> internalNamesToCatch() const;

    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    std::unique_ptr<AbstractStreamSocket> m_tcpSocket;

    QnMutex m_mutex;

    std::atomic<bool> m_terminated{false};
};

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
