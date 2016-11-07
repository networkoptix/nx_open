#pragma once

#include <map>
#include <set>

#include <QtWebSockets/QWebSocket>

#include "flir_websocket_proxy.h"

#include <plugins/common_interfaces/abstract_io_manager.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/safe_direct_connection.h>
#include <nx/network/http/asynchttpclient.h>
#include <core/resource/resource_data.h>

class FlirWebSocketIoManager :
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public QnAbstractIOManager
{
    Q_OBJECT

    struct AlarmEvent
    {
        QString alarmId;
        bool alarmState = 0;
    };

    struct ServerWhoAmIResponse
    {
        qint64 returnCode;
        QString returnString;
        qint64 sessionId;
        bool owner;
        QString ip;
    };

public:
    FlirWebSocketIoManager(QnResource* resource);

    virtual ~FlirWebSocketIoManager();

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

    virtual void terminate() override;

private slots:
    void at_controlWebSocketConnected();
    void at_controlWebSocketDisconnected();
    void at_notificationWebSocketConnected();
    void at_notificationWebSocketDisconnected();
    void at_controlWebSocketError(QAbstractSocket::SocketError error);
    void at_notificationWebSocketError(QAbstractSocket::SocketError error);

private:

    using NexusSettingGroup = std::map<QString, QString>;

    struct NexusServerStatus
    {
        std::map<QString, NexusSettingGroup> settings;
        bool isNexusServerEnabled = true;
    };

    enum class InitState
    {
        initial,
        nexusServerStatusRequested,
        nexusServerEnabled,
        controlSocketConnected,
        sessionIdObtained,
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
    NexusServerStatus parseNexusServerStatusResponse(const QString& response) const;

    void tryToEnableNexusServer();

    void connectWebsocket(
        const QString& path,
        FlirWebSocketProxy* proxy,
        std::chrono::milliseconds delay = std::chrono::milliseconds(0));

    void connectControlWebsocket(std::chrono::milliseconds delay = std::chrono::milliseconds(0));
    void connectNotificationWebSocket();
    void requestSessionId();
    ServerWhoAmIResponse parseControlMessage(const QString& message);

    QString buildNotificationSubscriptionPath() const; 
    QString buildNotificationSubscriptionParamString(const QString& subscriptionType) const;
    
    void handleNotification(const QString& message);
    bool isThgObjectNotificationType(const QString& notificationType) const;
    AlarmEvent parseNotification(const QString& message, bool *outStatus) const;
    AlarmEvent parseThgObjectNotification(const QStringList& notificationParts, bool* outStatus) const;    
    AlarmEvent parseAlarmNotification(const QStringList& notificationParts, bool* outStatus) const; //< maybe it's worth to subscribe to $IO not $ALARM

    qint64 parseFlirDateTime(const QString& dateTime, bool* outStatus);
    void checkAndNotifyIfNeeded(const AlarmEvent& notification);

    void sendKeepAlive();

private:
    QnResource* m_resource;
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
};
