#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

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
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) override;
    virtual void updateResource(const QnResourcePtr &resource) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
    virtual void processResources(const QnResourceList& resources) override;

private slots:
    void at_remotePeerFound(ec2::ApiPeerAliveData data);
    void at_remotePeerLost(ec2::ApiPeerAliveData data);
    void at_systemNameChangeRequested(const QString &systemName);
private:
    void updateServerTmpStatus(const QUuid& id, Qn::ResourceStatus status);
    void checkForTmpStatus(const QnResourcePtr& resource);

private:
    QSharedPointer<QnIncompatibleServerWatcher> m_incompatibleServerWatcher;
    bool m_connected;
    bool m_holdConnection;
};

#endif // _client_event_manager_h
