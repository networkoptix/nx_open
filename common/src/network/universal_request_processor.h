#ifndef __UNIVERSAL_REQUEST_PROCESSOR_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_H__

#include "utils/network/tcp_connection_processor.h"
#include "universal_tcp_listener.h"

class QnUniversalRequestProcessorPrivate;

class QnUniversalRequestProcessor: public QnTCPConnectionProcessor
{
public:
    QnUniversalRequestProcessor(QSharedPointer<AbstractStreamSocket> socket,
                                QnUniversalTcpListener* owner, bool needAuth);

    QnUniversalRequestProcessor(QnUniversalRequestProcessorPrivate* priv,
                                QSharedPointer<AbstractStreamSocket> socket,
                                QnUniversalTcpListener* owner, bool needAuth);

    virtual ~QnUniversalRequestProcessor();

    static void setUnauthorizedPageBody(const QByteArray& value);
    static QByteArray unauthorizedPageBody();

protected:
    virtual void run() override;
    virtual void pleaseStop() override;
    bool processRequest(QnUniversalTcpListener::InstanceFunc handler = nullptr);
    bool authenticate(QnUuid* userId);

private:
    bool prcessRequestLockless();
    Q_DECLARE_PRIVATE(QnUniversalRequestProcessor);
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_H__
