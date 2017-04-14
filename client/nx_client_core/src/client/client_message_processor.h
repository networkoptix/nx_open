#pragma once

#include <api/common_message_processor.h>

#include <client/client_connection_status.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;

public:
    explicit QnClientMessageProcessor(QObject* parent = nullptr);
    virtual void init(const ec2::AbstractECConnectionPtr& connection) override;

    const QnClientConnectionStatus* connectionStatus() const;

    void setHoldConnection(bool holdConnection);

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;

    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        Qn::ResourceStatus status,
        ec2::NotificationSource source) override;
    virtual void updateResource(const QnResourcePtr &resource, ec2::NotificationSource source) override;
    virtual void onGotInitialNotification(const ec2::ApiFullInfoData& fullData) override;

    virtual void handleRemotePeerFound(const ec2::ApiPeerAliveData &data) override;
    virtual void handleRemotePeerLost(const ec2::ApiPeerAliveData &data) override;
private:
    QnClientConnectionStatus m_status;
    bool m_connected;
    bool m_holdConnection;
};

#define qnClientMessageProcessor \
    static_cast<QnClientMessageProcessor*>(commonModule()->messageProcessor())