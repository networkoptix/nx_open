#pragma once

#include <client/client_message_processor.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnMobileClientMessageProcessor : public QnClientMessageProcessor
{
    Q_OBJECT
    typedef QnClientMessageProcessor base_type;

public:
    QnMobileClientMessageProcessor(QObject* parent = nullptr);

    bool isConnected() const;

    virtual void updateResource(const QnResourcePtr &resource, ec2::NotificationSource source) override;

protected:
    virtual QnResourceFactory* getResourceFactory() const override;

private:
    void updateMainServerApiUrl(const QnMediaServerResourcePtr& server);
};

#define qnMobileClientMessageProcessor \
    static_cast<QnMobileClientMessageProcessor*>(commonModule()->messageProcessor())
