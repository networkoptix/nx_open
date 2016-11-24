
#include "ec2_connection_processor.h"

#include <QtCore/QElapsedTimer>

#include <network/http_connection_listener.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>


Ec2ConnectionProcessor::Ec2ConnectionProcessor(
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(socket),
    m_owner(owner),
    m_processor(nullptr)
{
    setObjectName(QLatin1String("Ec2ConnectionProcessor"));
}

Ec2ConnectionProcessor::~Ec2ConnectionProcessor()
{
    stop();
}

//bool Ec2ConnectionProcessor::authenticate(Qn::UserAccessData* accessRights, bool *noAuth)
//{
//    Q_D(Ec2ConnectionProcessor);
//
//    int retryCount = 0;
//    bool logReported = false;
//    if (d->needAuth)
//    {
//        QUrl url = getDecodedUrl();
//        const bool isProxy = isProxyForCamera(d->request);
//        QElapsedTimer t;
//        t.restart();
//        AuthMethod::Value usedMethod = AuthMethod::noAuth;
//        Qn::AuthResult authResult;
//        while ((authResult = qnAuthHelper->authenticate(d->request, d->response, isProxy, accessRights, &usedMethod)) != Qn::Auth_OK)
//        {
//            nx_http::insertOrReplaceHeader(
//                &d->response.headers,
//                nx_http::HttpHeader(Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(authResult).toUtf8()));
//
//            int retryThreshold = 0;
//            if (authResult == Qn::Auth_WrongDigest)
//                retryThreshold = MAX_AUTH_RETRY_COUNT;
//            else if (d->authenticatedOnce)
//                retryThreshold = 2; // Allow two more try if password just changed (QT client need it because of password cache)
//            if (retryCount >= retryThreshold && !logReported && authResult != Qn::Auth_WrongInternalLogin)
//            {
//                logReported = true;
//                auto session = authSession();
//                session.id = QnUuid::createUuid();
//                qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));
//            }
//
//            if (!d->socket->isConnected())
//                return false;   //connection has been closed
//
//            nx_http::StatusCode::Value httpResult;
//            QByteArray msgBody;
//            if (isProxy)
//            {
//                msgBody = STATIC_PROXY_UNAUTHORIZED_HTML;
//                httpResult = nx_http::StatusCode::proxyAuthenticationRequired;
//            }
//            else if (authResult == Qn::Auth_Forbidden)
//            {
//                msgBody = STATIC_FORBIDDEN_HTML;
//                httpResult = nx_http::StatusCode::forbidden;
//            }
//            else
//            {
//                if (usedMethod & m_unauthorizedPageForMethods)
//                    msgBody = unauthorizedPageBody();
//                else
//                    msgBody = STATIC_UNAUTHORIZED_HTML;
//                httpResult = nx_http::StatusCode::unauthorized;
//            }
//            sendUnauthorizedResponse(httpResult, msgBody);
//
//
//            if (++retryCount > MAX_AUTH_RETRY_COUNT)
//            {
//                return false;
//            }
//            while (t.elapsed() < AUTH_TIMEOUT && d->socket->isConnected())
//            {
//                if (readRequest())
//                {
//                    parseRequest();
//                    break;
//                }
//            }
//            if (t.elapsed() >= AUTH_TIMEOUT || !d->socket->isConnected())
//                return false; // close connection
//        }
//        if (qnAuthHelper->restrictionList()->getAllowedAuthMethods(d->request) & AuthMethod::noAuth)
//            *noAuth = true;
//        if (usedMethod != AuthMethod::noAuth)
//            d->authenticatedOnce = true;
//    }
//    return true;
//}

void Ec2ConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);

    initSystemThreadId();

    if (!readRequest())
        return;

    QElapsedTimer t;
    t.restart();
    bool ready = true;
    bool isKeepAlive = false;

    while (1)
    {
        if (ready)
        {
            t.restart();
            parseRequest();

            /*auto handler =*/ m_owner->findHandler(d->protocol, d->request);
            bool noAuth = true;
            //if (handler && !authenticate(&d->accessRights, &noAuth))
            //    return;

            isKeepAlive = isConnectionCanBePersistent();


            // getting a new handler inside is necessary due to possibility of
            // changing request during authentication
            if (!processRequest(noAuth))
            {
                QByteArray contentType;
                //int rez = redirectTo(QnTcpListener::defaultPage(), contentType);
                sendResponse(nx_http::StatusCode::badRequest, contentType);
            }
        }

        if (!d->socket)
            break; // processor has token socket ownership

        if (!isKeepAlive || t.elapsed() >= KEEP_ALIVE_TIMEOUT || !d->socket->isConnected())
            break;

        ready = readRequest();
    }
    if (d->socket)
        d->socket->close();
}

bool Ec2ConnectionProcessor::processRequest(bool noAuth)
{
    Q_D(QnTCPConnectionProcessor);

    QnMutexLocker lock(&m_mutex);
    if (auto handler = m_owner->findHandler(d->protocol, d->request))
        m_processor = handler(d->socket, m_owner);
    else
        return false;

    d->accessRights = Qn::kSystemAccess;

    auto restProcessor = dynamic_cast<QnRestConnectionProcessor*>(m_processor);
    if (restProcessor)
        restProcessor->setAuthNotRequired(noAuth);
    if (!needToStop())
    {
        copyClientRequestTo(*m_processor);
        if (m_processor->isTakeSockOwnership())
            d->socket.clear(); // some of handlers have addition thread and depend of socket destructor. We should clear socket immediately to prevent race condition
        m_processor->execute(m_mutex);
        if (!m_processor->isTakeSockOwnership())
            m_processor->releaseSocket();
        else
            d->socket.clear(); // some of handlers set ownership dynamically during a execute call. So, check it again.
    }

    delete m_processor;
    m_processor = nullptr;

    return true;
}

void Ec2ConnectionProcessor::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    QnTCPConnectionProcessor::pleaseStop();
}
