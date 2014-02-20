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
    virtual void loadRuntimeInfo(const QnMessage &message) override;
    virtual void handleConnectionOpened(const QnMessage &message) override;
    virtual void handleConnectionClosed(const QString &errorString) override;
    virtual void handleMessage(const QnMessage &message) override;
    virtual void init(const QUrl &url, const QString &authKey, int reconnectTimeout) override;
private:
    void updateResource(const QnResourcePtr& resource);
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
