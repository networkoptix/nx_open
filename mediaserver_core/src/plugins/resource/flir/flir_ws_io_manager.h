#pragma once

#include <map>

#include <QtWebSockets/QWebSocket>

#include <plugins/common_interfaces/abstract_io_manager.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/safe_direct_connection.h>
#include <nx/network/http/asynchttpclient.h>

class FlirWsIOManager :
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public QnAbstractIOManager
{
    Q_OBJECT

    struct FlirAlarmNotification
    {
        int deviceId;
        int health;
        int lastBIT;
        quint64 timestamp;
        int deviceType;
        int sourceIndex;
        QString sourceName;
        QString timestamp2;
        int state;
        double latitude;
        double longitude;
        double altitude;
        int autoacknowledge;
    };

    struct FlirServerWhoAmIResponse
    {
        qint64 returnCode;
        QString returnString;
        qint64 sessionId;
        bool owner;
        QString ip;
    };

public:
    FlirWsIOManager(QnResource* resource);

    virtual ~FlirWsIOManager();

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

    enum class InitState
    {
        initial,
        settingsRequested,
        nexusServerEnabled,
        controlSocketConnected,
        sessionIdObtained,
        notificationSocketConnected,
        subscribed,
        error
    };

    void routeIOMonitoringInitialization(InitState newState);

    bool initHttpClient();

    bool tryToEnableNexusServer();
    bool tryToGetNexusSettings();

    void connectWebsocket(const QString& path, QWebSocket* socket);
    void connectControlWebsocket();
    void connectNotificationWebSocket();
    void requestSessionId();
    FlirServerWhoAmIResponse parseControlMessage(const QString& message);

    QString buildNotificationSubscriptionPath() const; 
    QString buildNotificationSubscriptionParamString(const QString& subscriptionType) const;

    void subscribeToNotifications();
    void handleNotification(const QString& message);
    FlirAlarmNotification parseNotification(const QString& message, bool *outStatus);
    qint64 parseFlirDateTime(const QString& dateTime, bool* outStatus);
    void checkAndNotifyIfNeeded(const FlirAlarmNotification& notification);

    void sendKeepAlive();

private:
    QnResource* m_resource;
    InitState m_initializationState;
    qint64 m_nexusSessionId;
    nx::utils::TimerId m_timerId;
    std::atomic<bool> m_monitoringIsInProgress;
    std::unique_ptr<QWebSocket> m_controlWebSocket;
    std::unique_ptr<QWebSocket> m_notificationWebSocket;
    nx_http::AsyncHttpClientPtr m_asyncHttpClient;

    InputStateChangeCallback m_stateChangeCallback;
    NetworkIssueCallback m_networkIssueCallback;

    mutable std::map<QString, bool> m_alarmStates; //< TODO: #dmishin mutable looks a little bit odd here, remove it.

    bool m_isNexusServerEnabled;
    quint16 m_nexusPort;

    QnMutex m_mutex;
};
