#pragma once

#include <client/client_message_processor.h>

#include <nx/vms/api/data_fwd.h>

class QnIncompatibleServerWatcher;

class QnDesktopClientMessageProcessor: public QnClientMessageProcessor
{
    Q_OBJECT
    using base_type = QnClientMessageProcessor;
public:
    explicit QnDesktopClientMessageProcessor(QObject* parent = nullptr);
    virtual ~QnDesktopClientMessageProcessor() override;

    QnIncompatibleServerWatcher *incompatibleServerWatcher() const;

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void onGotInitialNotification(const nx::vms::api::FullInfoData& fullData) override;
    virtual QnResourceFactory* getResourceFactory() const override;

private slots:
    void at_gotInitialDiscoveredServers(
        const nx::vms::api::DiscoveredServerDataList& discoveredServers);

private:
    QScopedPointer<QnIncompatibleServerWatcher> m_incompatibleServerWatcher;
};

#define qnDesktopClientMessageProcessor \
    static_cast<QnDesktopClientMessageProcessor*>(commonModule()->messageProcessor())
