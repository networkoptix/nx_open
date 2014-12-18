#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <client/client_connection_status.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>

class QnIncompatibleServerWatcher;

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnClientMessageProcessor();
    virtual void init(const ec2::AbstractECConnectionPtr& connection) override;

    void setHoldConnection(bool holdConnection);

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;

    virtual void onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) override;
    virtual void updateResource(const QnResourcePtr &resource) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
    virtual void resetResources(const QnResourceList& resources) override;

    virtual void handleRemotePeerFound(const ec2::ApiPeerAliveData &data) override;
    virtual void handleRemotePeerLost(const ec2::ApiPeerAliveData &data) override;

private slots:
    void at_systemNameChangeRequested(const QString &systemName);

private:
    QnIncompatibleServerWatcher *m_incompatibleServerWatcher;
    QnClientConnectionStatus m_status;
    bool m_connected;
    bool m_holdConnection;
};

#endif // _client_event_manager_h
