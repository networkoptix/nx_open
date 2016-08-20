#ifndef __UNIVERSAL_REQUEST_PROCESSOR_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_H__

#include "network/tcp_connection_processor.h"
#include "universal_tcp_listener.h"
#include "authenticate_helper.h"

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

    static void setUnauthorizedPageBody(const QByteArray& value, AuthMethod::Values methods);
    static QByteArray unauthorizedPageBody();

    static bool isProxy(const nx_http::Request& request);
    static bool isProxyForCamera(const nx_http::Request& request);
    static bool isCloudRequest(const nx_http::Request& request);
    static bool needStandardProxy(const nx_http::Request& request);
protected:
    virtual void run() override;
    virtual void pleaseStop() override;
    bool processRequest(bool noAuth);
    bool authenticate(Qn::UserAccessData* accessRights, bool *noAuth);

private:
    bool prcessRequestLockless();
    Q_DECLARE_PRIVATE(QnUniversalRequestProcessor);
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_H__
