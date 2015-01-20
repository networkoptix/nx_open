#ifndef __UNIVERSAL_REQUEST_PROCESSOR_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_H__

#include "utils/network/tcp_connection_processor.h"

class QnUniversalRequestProcessorPrivate;

class QnUniversalRequestProcessor: public QnTCPConnectionProcessor
{
public:
    QnUniversalRequestProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner, bool needAuth);
    QnUniversalRequestProcessor(QnUniversalRequestProcessorPrivate* priv, QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner, bool needAuth);
    virtual ~QnUniversalRequestProcessor();

protected:
    virtual void run() override;
    virtual void pleaseStop() override;
    void processRequest();
    bool authenticate(QnUuid* userId);
private:
    Q_DECLARE_PRIVATE(QnUniversalRequestProcessor);
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_H__
