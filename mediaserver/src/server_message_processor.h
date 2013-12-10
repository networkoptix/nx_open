#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <QSharedPointer>

#include "api/message_source.h"
#include <api/common_message_processor.h>

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
private:
    bool m_tryDirectConnect;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
