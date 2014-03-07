#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource.h>
#include "nx_ec/impl/ec_api_impl.h"

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();
protected:
    virtual void onResourceStatusChanged(QnResourcePtr , QnResource::Status ) override;
    virtual void updateResource(QnResourcePtr resource) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
    virtual void init(ec2::AbstractECConnectionPtr connection);
private slots:
    void at_remotePeerFound(QnId id);
    void at_remotePeerLost(QnId id, bool isClient);
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
