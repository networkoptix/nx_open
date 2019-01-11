#ifndef __UNIVERSAL_REQUEST_PROCESSOR_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_H__

#include "network/tcp_connection_processor.h"
#include "universal_tcp_listener.h"

class QnUniversalRequestProcessorPrivate;

class QnUniversalRequestProcessor: public QnTCPConnectionProcessor
{
public:
    QnUniversalRequestProcessor(std::unique_ptr<nx::network::AbstractStreamSocket> socket,
                                QnUniversalTcpListener* owner, bool needAuth);

    QnUniversalRequestProcessor(QnUniversalRequestProcessorPrivate* priv,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
                                QnUniversalTcpListener* owner, bool needAuth);

    virtual ~QnUniversalRequestProcessor();

    static void setUnauthorizedPageBody(const QByteArray& value, nx::network::http::AuthMethod::Values methods);
    static QByteArray unauthorizedPageBody();
protected:
    virtual void run() override;
    virtual void pleaseStop() override;

    /*
     *
     */
    bool hasSecurityIssue();
    bool redirectToScheme(const char* scheme);
    bool processRequest(bool noAuth);
    bool authenticate(Qn::UserAccessData* accessRights, bool *noAuth);

private:
    bool prcessRequestLockless();
    Q_DECLARE_PRIVATE(QnUniversalRequestProcessor);
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_H__
