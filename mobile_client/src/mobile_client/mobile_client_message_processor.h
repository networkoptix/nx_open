#ifndef MOBILE_CLIENT_MESSAGE_PROCESSOR_H
#define MOBILE_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>
#include <core/resource/resource_fwd.h>

class QnMobileClientMessageProcessor : public QnCommonMessageProcessor {
    Q_OBJECT
    typedef QnCommonMessageProcessor base_type;

public:
    QnMobileClientMessageProcessor();
    virtual void init(const ec2::AbstractECConnectionPtr &connection) override;

    virtual void updateResource(const QnResourcePtr &resource) override;

protected:
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) override;

private slots:
    void at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy);
    void at_remotePeerLost(ec2::ApiPeerAliveData data, bool isProxy);

private:
    bool m_connected;
};

#endif // MOBILE_CLIENT_MESSAGE_PROCESSOR_H
