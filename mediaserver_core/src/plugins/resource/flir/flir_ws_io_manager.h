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
    void at_gotMessageOnNotificationWebSocket(const QString& message);

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

    void routeIOMonitoringInitializationUnsafe(InitState newState);

    void initIoPortStatesUnsafe();
    bool initHttpClientUnsafe();
    void resetSocketProxiesUnsafe();
    void reinitMonitoringUnsafe();

    QString getResourcesHostAddress() const;
    QAuthenticator getResourceAuth() const;
    QnResourceData getResourceData() const;
    int getPortNumberByPortId(const QString& portId) const;
    int getGpioModuleIdByPortId(const QString& portId) const;

    void tryToGetNexusServerStatusUnsafe();
    void tryToEnableNexusServerUnsafe();
    void requestSessionIdUnsafe();
    void requestRemoteControlUnsafe();

    void connectWebsocketUnsafe(
        const QString& path,
        nexus::WebSocketProxy* proxy,
        std::chrono::milliseconds delay = std::chrono::milliseconds(0));
    void connectControlWebsocketUnsafe(std::chrono::milliseconds delay = std::chrono::milliseconds(0));
    void connectNotificationWebSocketUnsafe();

    QString buildNotificationSubscriptionPath() const; 
    
    void handleServerWhoAmIResponseUnsafe(const nexus::Response& response);
    void handleRemoteControlRequestResponseUnsafe(const nexus::Response& response);
    void handleRemoteControlReleaseResponseUnsafe(const nexus::Response& response);
    void handleIoSensorOutputStateSetResponseUnsafe(const nexus::Response& response);
    void handleServerStatusResponseUnsafe(nx_http::AsyncHttpClientPtr httpClient);
    void handleServerEnableResponseUnsafe(nx_http::AsyncHttpClientPtr httpClient);

    void sendKeepAliveUnsafe();

    void checkAndNotifyIfNeeded(const nexus::Notification& notification);

private:
    QnVirtualCameraResource* m_resource;
    InitState m_initializationState;
    qint64 m_nexusSessionId;
    nx::utils::TimerId m_timerId;
    nx::utils::TimerId m_keepAliveTimerId;
    std::atomic<bool> m_monitoringIsInProgress;

    nexus::WebSocketProxy* m_controlProxy;
    nexus::WebSocketProxy* m_notificationProxy;

    nx_http::AsyncHttpClientPtr m_asyncHttpClient;

    InputStateChangeCallback m_stateChangeCallback;
    NetworkIssueCallback m_networkIssueCallback;

    std::map<QString, int> m_alarmStates;
    QnIOPortDataList m_inputs;
    QnIOPortDataList m_outputs;

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
