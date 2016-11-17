#pragma once

#include <map>
#include <set>

#include <QtWebSockets/QWebSocket>

#include "flir_websocket_proxy.h"
#include "flir_nexus_response.h"
#include "flir_nexus_parsing_utils.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_data.h>
#include <plugins/common_interfaces/abstract_io_manager.h>
#include <nx/utils/timer_manager.h>
#include <nx/network/http/asynchttpclient.h>

namespace nx{
namespace plugins{
namespace flir{

class WebSocketIoManager :
    public QObject,
    public QnAbstractIOManager
{
    Q_OBJECT

public:
    WebSocketIoManager(QnVirtualCameraResource* resource);

    virtual ~WebSocketIoManager();

    virtual bool startIOMonitoring() override;

    virtual void stopIOMonitoring() override;

    virtual bool setOutputPortState(const QString& portId, bool isActive) override;

    virtual bool isMonitoringInProgress() const override;

    virtual QnIOPortDataList getOutputPortList() const override;

    virtual QnIOPortDataList getInputPortList() const override;

    virtual QnIOStateDataList getPortStates() const override;

    virtual nx_io_managment::IOPortState getPortDefaultState(const QString& portId) const override;

    virtual void setPortDefaultState(const QString& portId, nx_io_managment::IOPortState state) override;

    virtual void setInputPortStateChangeCallback(InputStateChangeCallback callback) override;

    virtual void setNetworkIssueCallback(NetworkIssueCallback callback) override;

    virtual void terminate();

private slots:
    void at_controlWebSocketConnected();
    void at_controlWebSocketDisconnected();

    void at_notificationWebSocketConnected();
    void at_notificationWebSocketDisconnected();

    void at_controlWebSocketError(QAbstractSocket::SocketError error);
    void at_notificationWebSocketError(QAbstractSocket::SocketError error);

    void at_gotMessageOnControlSocket(const QString& message);

private:
    enum class InitState
    {
        initial,
        nexusServerStatusRequested,
        nexusServerEnabled,
        controlSocketConnected,
        sessionIdObtained,
        remoteControlObtained,
        subscribed,
        error
    };

    void routeIOMonitoringInitialization(InitState newState);

    bool initHttpClient();
    void resetSocketProxies();

    QString getResourcesHostAddress() const;
    QAuthenticator getResourceAuth() const;
    QnResourceData getResourceData() const;

    void tryToGetNexusServerStatus();

    void tryToEnableNexusServer();

    void connectWebsocket(
        const QString& path,
        FlirWebSocketProxy* proxy,
        std::chrono::milliseconds delay = std::chrono::milliseconds(0));

    void connectControlWebsocket(std::chrono::milliseconds delay = std::chrono::milliseconds(0));
    void connectNotificationWebSocket();
    void requestSessionId();
    void requestRemoteControl();

    QString buildNotificationSubscriptionPath() const; 
    
    void handleServerWhoAmIResponse(const nexus::Response& response);
    void handleRemoteControlRequestResponse(const nexus::Response& response);
    void handleRemoteControlReleaseResponse(const nexus::Response& response);
    void handleIoSensorOutputStateSetResponse(const nexus::Response& response);

    void handleNotification(const QString& message);

    void checkAndNotifyIfNeeded(const nexus::Notification& notification);

    void sendKeepAlive();

    int getPortNumberByPortId(const QString& portId) const;
    int getGpioModuleIdByPortId(const QString& portId) const;

private:
    QnVirtualCameraResource* m_resource;
    InitState m_initializationState;
    qint64 m_nexusSessionId;
    nx::utils::TimerId m_timerId;
    nx::utils::TimerId m_keepAliveTimerId;
    std::atomic<bool> m_monitoringIsInProgress;

    FlirWebSocketProxy* m_controlProxy;
    FlirWebSocketProxy* m_notificationProxy;

    nx_http::AsyncHttpClientPtr m_asyncHttpClient;

    InputStateChangeCallback m_stateChangeCallback;
    NetworkIssueCallback m_networkIssueCallback;

    mutable std::map<QString, bool> m_alarmStates; //< TODO: #dmishin mutable looks a little bit odd here, remove it.

    bool m_isNexusServerEnabled;
    bool m_nexusServerHasJustBeenEnabled;
    quint16 m_nexusPort;

    mutable QnMutex m_mutex;
    mutable QnMutex m_setOutputStateMutex;
    mutable QnWaitCondition m_setOutputStateRequestCondition;
};

} //namespace flir
} //namespace plugins
} //namespace nx
