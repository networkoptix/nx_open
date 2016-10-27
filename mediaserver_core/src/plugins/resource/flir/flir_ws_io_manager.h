#pragma once

#include <QtWebSockets/QWebSocket>

#include <plugins/common_interfaces/abstract_io_manager.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/safe_direct_connection.h>

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
        int timestamp2;
        double latitude;
        double longitude;
        double altitude;
        int autoacknowledge;
    };

public:
    FlirWsIOManager();

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
    void at_connected();
    void at_disconnected();

private:
    bool initiateWsConnection();
    void requestControlToken();
    qint64 parseControlMessage(const QString& message);

    QString buildNotificationSubsctionString(const QString& subscriptionType);
    void subscribeToNotifications();
    FlirAlarmNotification parseNotification(const QString& message);
    quint64 parseFlirDateTime(const QString& dateTime);

    void sendKeepAlive();

private:
    qint64 m_nexusSessionId;
    nx::utils::TimerId m_timerId;
    std::atomic<bool> m_monitoringIsInProgress;
    std::unique_ptr<QWebSocket> m_controlWebSocket;
    std::unique_ptr<QWebSocket> m_notificationWebSocket;

    InputStateChangeCallback m_stateChangeCallback;
    NetworkIssueCallback m_networkIssueCallback;

    QnMutex m_mutex;

};
