#ifndef __BACK_PROXY_CONNECTION_PROCESSOR_H__
#define __BACK_PROXY_CONNECTION_PROCESSOR_H__

#include "network/universal_request_processor.h"
#include "proxy_connection.h"

class QnProxyReceiverConnectionPrivate;

class QnProxyReceiverConnection: public QnTCPConnectionProcessor
{
public:
    QnProxyReceiverConnection(QSharedPointer<AbstractStreamSocket> socket,
                              QnUniversalTcpListener* owner);
    virtual bool isTakeSockOwnership() const override;
protected:
    virtual void run();
private:
    Q_DECLARE_PRIVATE(QnProxyReceiverConnection);
};

#endif // __BACK_PROXY_CONNECTION_PROCESSOR_H__
