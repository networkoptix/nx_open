#ifndef __UNIVERSAL_REQUEST_PROCESSOR_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_H__

#include "utils/network/tcp_connection_processor.h"

class QnUniversalRequestProcessorPrivate;

class QnUniversalRequestProcessor: public QnTCPConnectionProcessor
{
public:
    QnUniversalRequestProcessor(AbstractStreamSocket* socket, QnTcpListener* owner);
    QnUniversalRequestProcessor(QnUniversalRequestProcessorPrivate* priv, AbstractStreamSocket* socket, QnTcpListener* owner);
    virtual ~QnUniversalRequestProcessor();

protected:
    virtual void run() override;
    virtual void pleaseStop() override;
    void processRequest();
private:
    Q_DECLARE_PRIVATE(QnUniversalRequestProcessor);
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_H__
