#pragma once

#include <QtWebSockets/QWebSocket>

#include <plugins/common_interfaces/abstract_io_manager.h>

class FlirWsIOManager : public nx_io_managment::QnAbstractIOManager
{
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
    void parseControlMessage();
    void parseNotification(const QString& message);
    void sendKeepAlive();

private:
    QString m_nexusSessionId;
    std::atomic<bool> m_monitoringIsInProgress;
    std::unique_ptr<QWebSocket> m_controlWebSocket;
    std::unique_ptr<QWebSocket> m_notificationWebSocket;

}
