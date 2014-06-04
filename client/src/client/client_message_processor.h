#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>

class QnIncompatibleServerAdder;

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnClientMessageProcessor();
    virtual void init(ec2::AbstractECConnectionPtr connection) override;
protected:
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) override;
    virtual void updateResource(const QnResourcePtr &resource) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
    virtual void processResources(const QnResourceList& resources) override;
private:
    bool m_opened;
    QnIncompatibleServerAdder *m_incompatibleServerAdder;
private slots:
    void at_remotePeerFound(ec2::ApiServerAliveData, bool isProxy);
    void at_remotePeerLost(ec2::ApiServerAliveData, bool isProxy);

    void at_systemNameChangeRequested(const QString &systemName);

private:
    void determineOptimalIF(const QnMediaServerResourcePtr &resource);
    void updateServerTmpStatus(const QnId& id, QnResource::Status status);
    void checkForTmpStatus(QnResourcePtr resource);
};

#endif // _client_event_manager_h
