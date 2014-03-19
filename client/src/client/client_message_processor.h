#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnClientMessageProcessor();
    virtual void init(ec2::AbstractECConnectionPtr connection) override;
protected:
    virtual void onResourceStatusChanged(QnResourcePtr resource, QnResource::Status status) override;
    virtual void updateResource(QnResourcePtr resource) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
private:
    bool m_opened;
private slots:
    void at_remotePeerFound(QnId id, bool isClient, bool isProxy);
    void at_remotePeerLost(QnId id, bool isClient, bool isProxy);
private:
    void determineOptimalIF(const QnMediaServerResourcePtr &resource);
    void updateTmpStatus(const QnId& id, QnResource::Status status);
};

#endif // _client_event_manager_h
