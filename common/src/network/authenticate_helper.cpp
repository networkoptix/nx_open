
#include "authenticate_helper.h"

#include <utils/common/uuid.h>
#include <QtCore/QCryptographicHash>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include "utils/common/util.h"
#include "utils/common/synctime.h"
#include <utils/network/simple_http_client.h>
#include <utils/match/wildcard.h>
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "core/resource/media_server_resource.h"


////////////////////////////////////////////////////////////
//// class QnAuthMethodRestrictionList
////////////////////////////////////////////////////////////
QnAuthMethodRestrictionList::QnAuthMethodRestrictionList()
{
}

unsigned int QnAuthMethodRestrictionList::getAllowedAuthMethods( const nx_http::Request& request ) const
{
    QString path = request.requestLine.url.path();
    //TODO #ak replace mid and chop with single midRef call
    while (path.startsWith(lit("//")))
        path = path.mid(1);
    while (path.endsWith(L'/'))
        path.chop(1);
    unsigned int allowed = AuthMethod::cookie | AuthMethod::http | AuthMethod::videowall;   //by default
    for( std::pair<QString, unsigned int> allowRule: m_allowed )
    {
        if( !wildcardMatch( allowRule.first, path ) )
            continue;
        allowed |= allowRule.second;
    }

    for( std::pair<QString, unsigned int> denyRule: m_denied )
    {
        if( !wildcardMatch( denyRule.first, path ) )
            continue;
        allowed &= ~denyRule.second;
    }

    return allowed;
}

void QnAuthMethodRestrictionList::allow( const QString& pathMask, AuthMethod::Value method )
{
    m_allowed[pathMask] = method;
}

void QnAuthMethodRestrictionList::deny( const QString& pathMask, AuthMethod::Value method )
{
    m_denied[pathMask] = method;
}


////////////////////////////////////////////////////////////
//// class QnAuthHelper
////////////////////////////////////////////////////////////
QnAuthHelper* QnAuthHelper::m_instance;

static const qint64 NONCE_TIMEOUT = 1000000ll * 60 * 5;
static const QString REALM(lit("NetworkOptix"));

QnAuthHelper::QnAuthHelper()
{
    connect(qnResPool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceChanged(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
}

QnAuthHelper::~QnAuthHelper()
{
    disconnect(qnResPool, NULL, this, NULL);
}

void QnAuthHelper::initStaticInstance(QnAuthHelper* instance)
{
    delete m_instance;
    m_instance = instance;
}

QnAuthHelper* QnAuthHelper::instance()
{
    return m_instance;
}

bool QnAuthHelper::authenticate(const nx_http::Request& request, nx_http::Response& response, bool isProxy, QnUuid* authUserId)
{
    if (authUserId)
        *authUserId = QnUuid();

    const unsigned int allowedAuthMethods = m_authMethodRestrictionList.getAllowedAuthMethods( request );
    if( allowedAuthMethods == 0 )
        return false;   //NOTE assert?

    if( allowedAuthMethods & AuthMethod::noAuth )
        return true;

    if( allowedAuthMethods & AuthMethod::cookie )
    {
        const QString& cookie = QLatin1String(nx_http::getHeaderValue( request.headers, "Cookie" ));
        int customAuthInfoPos = cookie.indexOf(lit("authinfo="));
        if (customAuthInfoPos >= 0) {
            QString digest = cookie.mid(customAuthInfoPos + QByteArray("authinfo=").length(), 32);
            QMutexLocker lock(&m_sessionKeyMutex);
            if (doCustomAuthorization(digest.toLatin1(), response, m_sessionKey))
                return true;
            if (doCustomAuthorization(digest.toLatin1(), response, m_prevSessionKey))
                return true;
        }
    }

    if( allowedAuthMethods & AuthMethod::videowall )
    {
        const nx_http::StringType& videoWall_auth = nx_http::getHeaderValue( request.headers, "X-NetworkOptix-VideoWall" );
        if (!videoWall_auth.isEmpty())
            return (!qnResPool->getResourceById(QnUuid(videoWall_auth)).dynamicCast<QnVideoWallResource>().isNull());
    }

    if( allowedAuthMethods & AuthMethod::urlQueryParam )
    {
        QUrlQuery urlQuery( request.requestLine.url.query() );
        const QByteArray& urlQueryParam = urlQuery.queryItemValue( isProxy ? lit("proxy_auth") : lit("auth") ).toLatin1();
        if( !urlQueryParam.isEmpty() )
        {
            const QByteArray& decodedAuthQueryItem = QByteArray::fromBase64( urlQueryParam, QByteArray::Base64UrlEncoding );
            const QList<QByteArray>& authTokens = decodedAuthQueryItem.split( ':' );
            if( authTokens.size() == 3 )
            {
                if( authenticate( QString::fromUtf8(authTokens[0]), authTokens[2] ) )
                    return true;
            }
        }
    }

    if( allowedAuthMethods & AuthMethod::http )
    {
        const nx_http::StringType& authorization = isProxy
            ? nx_http::getHeaderValue( request.headers, "Proxy-Authorization" )
            : nx_http::getHeaderValue( request.headers, "Authorization" );
        if (authorization.isEmpty()) {
            addAuthHeader(response, isProxy);
            return false;
        }

        nx_http::StringType authType;
        nx_http::StringType authData;
        int pos = authorization.indexOf(L' ');
        if (pos > 0) {
            authType = authorization.left(pos).toLower();
            authData = authorization.mid(pos+1);
        }
        if (authType == "digest")
            return doDigestAuth(request.requestLine.method, authData, response, isProxy, authUserId);
        else if (authType == "basic")
            return doBasicAuth(authData, response, authUserId);
        else
            return false;
    }

    return false;   //failed to authorise request with any method
}

bool QnAuthHelper::authenticate(
    const QAuthenticator& auth,
    const nx_http::Response& response,
    nx_http::Request* const request,
    HttpAuthenticationClientContext* const authenticationCtx )
{
    const nx_http::BufferType& authHeaderBuf = nx_http::getHeaderValue(
        response.headers,
        response.statusLine.statusCode == nx_http::StatusCode::proxyAuthenticationRequired
            ? "Proxy-Authenticate"
            : "WWW-Authenticate" );
    if( !authHeaderBuf.isEmpty() )
    {
        nx_http::header::WWWAuthenticate authenticateHeader;
        if( !authenticateHeader.parse( authHeaderBuf ) )
            return false;
        authenticationCtx->authenticateHeader = std::move( authenticateHeader );
    }
    authenticationCtx->responseStatusCode = response.statusLine.statusCode;

    return authenticate(
        auth,
        request,
        authenticationCtx );
}

bool QnAuthHelper::authenticate(
    const QAuthenticator& auth,
    nx_http::Request* const request,
    const HttpAuthenticationClientContext* const authenticationCtx )
{
    if( authenticationCtx->authenticateHeader &&
        authenticationCtx->authenticateHeader.get().authScheme == nx_http::header::AuthScheme::digest )
    {
        nx_http::insertOrReplaceHeader(
            &request->headers,
            nx_http::parseHeader( CLSimpleHTTPClient::digestAccess(
                auth,
                QLatin1String(authenticationCtx->authenticateHeader.get().params.value("realm")),
                QLatin1String(authenticationCtx->authenticateHeader.get().params.value("nonce")),
                QLatin1String(request->requestLine.method),
                request->requestLine.url.toString(),
                authenticationCtx->responseStatusCode == nx_http::StatusCode::proxyAuthenticationRequired ).toLatin1() ) );
    }
    else if( !auth.user().isEmpty() )   //basic authorization or no authorization specified
    {
        nx_http::insertOrReplaceHeader(
            &request->headers,
            nx_http::parseHeader( CLSimpleHTTPClient::basicAuth(auth) ) );
    }
    
    return true;
}

bool QnAuthHelper::authenticate( const QString& login, const QByteArray& digest ) const
{
    QMutexLocker lock(&m_mutex);
    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toLower() == login.toLower())
            return user->getDigest() == digest;
    }
    //checking if it videowall connect
    return !qnResPool->getResourceById(QnUuid(login)).dynamicCast<QnVideoWallResource>().isNull();
}

QnAuthMethodRestrictionList* QnAuthHelper::restrictionList()
{
    return &m_authMethodRestrictionList;
}

QByteArray QnAuthHelper::createUserPasswordDigest( const QString& userName, const QString& password )
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QString(lit("%1:NetworkOptix:%2")).arg(userName, password).toLatin1());
    return md5.result().toHex();
}

QByteArray QnAuthHelper::createHttpQueryAuthParam( const QString& userName, const QString& password )
{
    //TODO #ak it is very bad that authQueryItem value is valid permanently
        //Note that mediaserver should allow such authentication for only some requests
    //TODO #ak encrypt authQueryItem or remove this authentication method at all?
    const QByteArray& digest = QnAuthHelper::createUserPasswordDigest( userName, password );
    const QByteArray& authQueryItem = (userName.toUtf8() + ":" + QByteArray::number(rand()) + ":" + digest).toBase64(QByteArray::Base64UrlEncoding);
    return authQueryItem;
}

//!Splits \a data by \a delimiter not closed within quotes
/*!
    E.g.: 
    \code
    one, two, "three, four"
    \endcode

    will be splitted to 
    \code
    one
    two
    "three, four"
    \endcode
*/
static QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter)
{
    bool quoted = false;
    QList<QByteArray> rez;
    if (data.isEmpty())
        return rez;

    int lastPos = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i] == '\"')
            quoted = !quoted;
        else if (data[i] == delimiter && !quoted)
        {
            rez << QByteArray(data.constData() + lastPos, i - lastPos);
            lastPos = i + 1;
        }
    }
    rez << QByteArray(data.constData() + lastPos, data.size() - lastPos);

    return rez;
}

bool QnAuthHelper::doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, bool isProxy, QnUuid* authUserId)
{
    const QList<QByteArray>& authParams = smartSplit(authData, ',');

    QByteArray userName;
    QByteArray response;
    QByteArray nonce;
    QByteArray realm;
    QByteArray uri;

    for (int i = 0; i < authParams.size(); ++i)
    {
        QByteArray data = authParams[i].trimmed();
        int pos = data.indexOf('=');
        if (pos == -1)
            return false;
        QByteArray key = data.left(pos);
        QByteArray value = data.mid(pos+1);
        value = value.mid(1, value.length()-2);
        if (key == "username")
            userName = value.toLower();
        else if (key == "response")
            response = value;
        else if (key == "nonce")
            nonce = value;
        else if (key == "realm")
            realm = value;
        else if (key == "uri")
            uri = value;
    }

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(method);
    md5Hash.addData(":");
    md5Hash.addData(uri);
    QByteArray ha2 = md5Hash.result().toHex();

    if (isNonceValid(nonce)) 
    {
        QMutexLocker lock(&m_mutex);
        for(const QnUserResourcePtr& user: m_users)
        {
            if (user->getName().toUtf8().toLower() == userName)
            {
                QByteArray dbHash = user->getDigest();

                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(dbHash);
                md5Hash.addData(":");
                md5Hash.addData(nonce);
                md5Hash.addData(":");
                md5Hash.addData(ha2);
                QByteArray calcResponse = md5Hash.result().toHex();

                if (calcResponse == response) {
                    if (authUserId)
                        *authUserId = user->getId();
                    return true;
                }
            }
        }

        // authenticate by media server auth_key
        for(const QnMediaServerResourcePtr& server: m_servers)
        {
            if (server->getId().toString().toUtf8().toLower() == userName)
            {
                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(server->getAuthKey().toUtf8());
                md5Hash.addData(":");
                md5Hash.addData(nonce);
                md5Hash.addData(":");
                md5Hash.addData(ha2);
                QByteArray calcResponse = md5Hash.result().toHex();

                if (calcResponse == response)
                    return true;
            }
        }

    }

    addAuthHeader(responseHeaders, isProxy);
    return false;
}

bool QnAuthHelper::doBasicAuth(const QByteArray& authData, nx_http::Response& /*response*/, QnUuid* authUserId)
{
    QByteArray digest = QByteArray::fromBase64(authData);
    int pos = digest.indexOf(':');
    if (pos == -1)
        return false;
    QByteArray userName = digest.left(pos).toLower();
    QByteArray password = digest.mid(pos+1);

    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toUtf8().toLower() == userName)
        {
            if (user->checkPassword(QString::fromUtf8(password)))
            {
                if (user->getDigest().isEmpty())
                    emit emptyDigestDetected(user, QString::fromUtf8(userName), QString::fromUtf8(password));
                if (authUserId)
                    *authUserId = user->getId();
                return true;
            }
        }
    }

    // authenticate by media server auth_key
    for(const QnMediaServerResourcePtr& server: m_servers)
    {
        if (server->getId().toString().toUtf8().toLower() == userName)
        {
            if (server->getAuthKey().toUtf8() == password)
                return true;
        }
    }

    return false;
}

bool QnAuthHelper::doCustomAuthorization(const QByteArray& authData, nx_http::Response& /*response*/, const QByteArray& sesionKey)
{
    QByteArray digestBin = QByteArray::fromHex(authData);
    QByteArray sessionKeyBin = QByteArray::fromHex(sesionKey);
    int size = qMin(digestBin.length(), sessionKeyBin.length());
    for (int i = 0; i < size; ++i)
        digestBin[i] = digestBin[i] ^ sessionKeyBin[i];
    QByteArray digest = digestBin.toHex();

    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getDigest() == digest)
            return true;
    }
    return false;
}

void QnAuthHelper::addAuthHeader(nx_http::Response& response, bool isProxy)
{
    QString auth(lit("Digest realm=\"%1\",nonce=\"%2\""));
	QByteArray headerName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    nx_http::insertOrReplaceHeader(
        &response.headers,
        nx_http::HttpHeader(headerName, auth.arg(REALM).arg(QLatin1String(getNonce())).toLatin1() ) );
}

bool QnAuthHelper::isNonceValid(const QByteArray& nonce) const
{
    qint64 n1 = nonce.toLongLong(0, 16);
    qint64 n2 = qnSyncTime->currentUSecsSinceEpoch();
    return qAbs(n2 - n1) < NONCE_TIMEOUT;
}

QByteArray QnAuthHelper::getNonce()
{
    return QByteArray::number(qnSyncTime->currentUSecsSinceEpoch() , 16);
}

void QnAuthHelper::at_resourcePool_resourceAdded(const QnResourcePtr & res)
{
    QMutexLocker lock(&m_mutex);

    QnUserResourcePtr user = res.dynamicCast<QnUserResource>();
    QnMediaServerResourcePtr server = res.dynamicCast<QnMediaServerResource>();
    if (user)
        m_users.insert(user->getId(), user);
    else if (server)
        m_servers.insert(server->getId(), server);
}

void QnAuthHelper::at_resourcePool_resourceRemoved(const QnResourcePtr &res)
{
    QMutexLocker lock(&m_mutex);

    m_users.remove(res->getId());
    m_servers.remove(res->getId());
}

void QnAuthHelper::setSessionKey(const QByteArray& value)
{
    QMutexLocker lock(&m_sessionKeyMutex);
    m_prevSessionKey = m_sessionKey;
    m_sessionKey = value;
}

QByteArray QnAuthHelper::symmetricalEncode(const QByteArray& data)
{
    static const QByteArray mask = QByteArray::fromHex("4453D6654C634636990B2E5AA69A1312"); // generated from guid
    static const int maskSize = mask.size();
    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result.data()[i] ^= mask.data()[i % maskSize];
    return result;
}
