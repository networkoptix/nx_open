#ifndef MOBILE_CLIENT_MESSAGE_PROCESSOR_H
#define MOBILE_CLIENT_MESSAGE_PROCESSOR_H

#include <client/client_message_processor.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnMobileClientMessageProcessor : public QnClientMessageProcessor {
    Q_OBJECT
    typedef QnClientMessageProcessor base_type;

public:
    QnMobileClientMessageProcessor();

    bool isConnected() const;

    virtual void updateResource(const QnResourcePtr &resource) override;

protected:
    virtual QnResourceFactory* getResourceFactory() const override;

private:
    void updateMainServerApiUrl(const QnMediaServerResourcePtr& server);
};

#endif // MOBILE_CLIENT_MESSAGE_PROCESSOR_H
