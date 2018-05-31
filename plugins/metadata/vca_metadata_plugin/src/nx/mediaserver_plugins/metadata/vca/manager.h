#pragma once

#include <chrono>

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/elapsed_timer_thread_safe.h>
#include <nx/utils/system_error.h>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_global.h>

#include <plugins/plugin_tools.h>

#include <nx/sdk/metadata/camera_manager.h>

#include <nx/network/aio/timer.h>
#include <nx/network/system_socket.h>

#include "common.h"
#include "plugin.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

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

    void treatMessage(int size);

    void onReceive(SystemError::ErrorCode, size_t);

    void onConnect(SystemError::ErrorCode code);

    void reconnectSocket();

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* eventTypeList, int eventTypeListSize) override;

    virtual nx::sdk::Error setHandler(nx::sdk::metadata::MetadataHandler* handler) override;

    bool isTimerNeeded() const;

    std::chrono::milliseconds timeTillCheck() const;

    void sendEventStartedPacket(const AnalyticsEventType& event) const;

    void sendEventStoppedPacket(const AnalyticsEventType& event) const;

    void onTimer();

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override {};

private:
    QUrl m_url;
    QAuthenticator m_auth;
    Plugin* m_plugin;
    QByteArray m_cameraManifest;
    ElapsedEvents m_eventsToCatch;
    QByteArray m_buffer;
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;

    //nx::network::TCPSocket* m_tcpSocket = nullptr;
    std::unique_ptr<nx::network::TCPSocket> m_tcpSocket;
    nx::network::aio::Timer m_stopEventTimer;
    nx::network::aio::Timer m_reconnectTimer;
    network::SocketAddress m_cameraAddress;
};

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
