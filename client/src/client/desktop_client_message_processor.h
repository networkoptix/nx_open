#pragma once

#include <client/client_message_processor.h>
#include <nx_ec/data/api_discovery_data.h>

class QnIncompatibleServerWatcher;

class QnDesktopClientMessageProcessor : public QnClientMessageProcessor
{
    Q_OBJECT

    typedef QnClientMessageProcessor base_type;

public:
    QnDesktopClientMessageProcessor();

    QnIncompatibleServerWatcher *incompatibleServerWatcher() const;

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;

    virtual QnResourceFactory* getResourceFactory() const override;
private slots:
    void at_gotInitialDiscoveredServers(const ec2::ApiDiscoveredServerDataList &discoveredServers);

private:
    QnIncompatibleServerWatcher *m_incompatibleServerWatcher;
};

#define qnDesktopClientMessageProcessor \
    static_cast<QnDesktopClientMessageProcessor*>(QnDesktopClientMessageProcessor::instance())
