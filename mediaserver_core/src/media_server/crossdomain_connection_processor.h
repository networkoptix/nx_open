#pragma once

#include <network/tcp_connection_processor.h>

class QnCrossdomainConnectionProcessorPrivate;

/**
 * Dynamically construct crossdomain.xml result
 * It's allow access from domains:
 * 1. All server interfaces
 * 2. Server public IP
 * 3. Cloud portal URL
 */

class QnCrossdomainConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnCrossdomainConnectionProcessor(QSharedPointer<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnCrossdomainConnectionProcessor();
protected:
    virtual void run() override;
private:
    Q_DECLARE_PRIVATE(QnCrossdomainConnectionProcessor);
};
