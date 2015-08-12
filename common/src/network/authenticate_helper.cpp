
#include "authenticate_helper.h"

#include <QtCore/QCryptographicHash>

#include <utils/common/app_info.h>
#include <utils/common/uuid.h>
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
#include "http/custom_headers.h"
#include <nx_ec/dummy_handler.h>


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
    unsigned int allowed = AuthMethod::cookie | AuthMethod::http | AuthMethod::videowall | AuthMethod::urlQueryParam;   //by default
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
static const qint64 COOKIE_EXPERATION_PERIOD = 3600;
static const QString COOKIE_DIGEST_AUTH( lit( "Authorization=Digest" ) );
static const QString TEMP_AUTH_KEY_NAME = lit( "authKey" );
static const nx_http::StringType URL_QUERY_AUTH_KEY_NAME = "auth";

const unsigned int QnAuthHelper::MAX_AUTHENTICATION_KEY_LIFE_TIME_MS = 60 * 60 * 1000;

QnAuthHelper::QnAuthHelper()
{
    connect(qnResPool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceChanged(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    m_digestNonceCache.setMaxCost(100);
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

bool QnAuthHelper::authenticate(const nx_http::Request& request, nx_http::Response& response, bool isProxy, QnUuid* authUserId, AuthMethod::Value* usedAuthMethod)
{
    if (authUserId)
        *authUserId = QnUuid();
    if (usedAuthMethod)
        *usedAuthMethod = AuthMethod::noAuth;

    const QUrlQuery urlQuery( request.requestLine.url.query() );

    const unsigned int allowedAuthMethods = m_authMethodRestrictionList.getAllowedAuthMethods( request );
    if( allowedAuthMethods == 0 )
        return false;   //NOTE assert?

    if( allowedAuthMethods & AuthMethod::noAuth )
        return true;

    {
        QMutexLocker lk( &m_mutex );
        if( urlQuery.hasQueryItem( TEMP_AUTH_KEY_NAME ) )
        {
            auto it = m_authenticatedPaths.find( urlQuery.queryItemValue( TEMP_AUTH_KEY_NAME ) );
            if( it != m_authenticatedPaths.end() &&
                it->second.path == request.requestLine.url.path() )
            {
                if (usedAuthMethod)
                    *usedAuthMethod = AuthMethod::tempUrlQueryParam;
                return true;
            }
        }
    }

    if( allowedAuthMethods & AuthMethod::videowall )
    {
        const nx_http::StringType& videoWall_auth = nx_http::getHeaderValue( request.headers, Qn::VIDEOWALL_GUID_HEADER_NAME );
        if (!videoWall_auth.isEmpty()) {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::videowall;
            return (!qnResPool->getResourceById<QnVideoWallResource>(QnUuid(videoWall_auth)).isNull());
        }
    }

    if( allowedAuthMethods & AuthMethod::urlQueryParam )
    {
        const QByteArray& authQueryParam = urlQuery.queryItemValue(
            isProxy ? lit( "proxy_auth" ) : QString::fromLatin1(URL_QUERY_AUTH_KEY_NAME) ).toLatin1();
        if( !authQueryParam.isEmpty() )
        {
            if( authenticateByUrl( authQueryParam, request.requestLine.method, authUserId ) )
            {
                if (usedAuthMethod)
                    *usedAuthMethod = AuthMethod::urlQueryParam;
                return true;
            }
        }
    }

    if( allowedAuthMethods & AuthMethod::cookie )
    {
        const QString& cookie = QLatin1String(nx_http::getHeaderValue( request.headers, "Cookie" ));
        int customAuthInfoPos = cookie.indexOf(COOKIE_DIGEST_AUTH);
        if (customAuthInfoPos >= 0) {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::cookie;
            return doCookieAuthorization("GET", cookie.toUtf8(), response, authUserId);
        }
    }

    if( allowedAuthMethods & AuthMethod::http )
    {
        const nx_http::StringType& authorization = isProxy
            ? nx_http::getHeaderValue( request.headers, "Proxy-Authorization" )
            : nx_http::getHeaderValue( request.headers, "Authorization" );
        const nx_http::StringType nxUserName = nx_http::getHeaderValue( request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME );
        if( authorization.isEmpty() )
        {
            QnUserResourcePtr userResource;
            if( !nxUserName.isEmpty() )
            {
                userResource = findUserByName( nxUserName );
                if( userResource )
                {
                    if( userResource->getDigest().isEmpty() )
                    {
                        //using basic authentication to allow fill user's digest
                        nx_http::insertOrReplaceHeader(
                            &response.headers,
                            nx_http::HttpHeader(
                                isProxy ? "Proxy-Authenticate" : "WWW-Authenticate",
                                lit("Basic realm=%1").arg(QnAppInfo::realm()).toLatin1() ) );
                        return false;
                    }
                    if( userResource->getRealm() != QnAppInfo::realm() )
                    {
                        //requesting client to recalculate user digest
                        nx_http::insertOrReplaceHeader(
                            &response.headers,
                            nx_http::HttpHeader( Qn::REALM_HEADER_NAME, QnAppInfo::realm().toLatin1() ) );
                    }
                }
            }

            addAuthHeader(
                response,
                userResource ? userResource->getRealm() : QnAppInfo::realm(),
                isProxy);
            return false;
        }

        nx_http::StringType authType;
        nx_http::StringType authData;
        int pos = authorization.indexOf(L' ');
        if (pos > 0) {
            authType = authorization.left(pos).toLower();
            authData = authorization.mid(pos+1);
        }

        bool authResult = false;
        if (authType == "digest") 
        {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::httpDigest;

            authResult = doDigestAuth(
                request.requestLine.method, authData, response, isProxy, authUserId, ',',
                std::bind(&QnAuthHelper::isNonceValid, this, std::placeholders::_1));
        }
        else if (authType == "basic") {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::httpBasic;
            authResult = doBasicAuth(authData, response, authUserId);
        }
        else {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::httpBasic;
            authResult = false;
        }
        if( authResult )
        {
            //checking whether client re-calculated ha1 digest
            const auto newHa1Digest = nx_http::getHeaderValue( request.headers, Qn::HA1_DIGEST_HEADER_NAME );
            const auto realm = nx_http::getHeaderValue( request.headers, Qn::REALM_HEADER_NAME );
            const auto cryptSha512Hash = nx_http::getHeaderValue( request.headers, Qn::CRYPT_SHA512_HASH_HEADER_NAME );
            if( newHa1Digest.isEmpty() || realm.isEmpty() || nxUserName.isEmpty() || cryptSha512Hash.isEmpty() )
                return authResult;

            QnUserResourcePtr userResource = findUserByName( nxUserName );
            if( !userResource || (userResource->getRealm() == QString::fromUtf8(realm)) )
                return authResult;
            //saving new user's digest
            //TODO #ak set following properties atomically
            userResource->setRealm( QString::fromUtf8(realm) );
            userResource->setDigest( newHa1Digest );
            userResource->setCryptSha512Hash( cryptSha512Hash );
            userResource->setHash( QByteArray() );
            //saving user to DB. NOTE this code is server-side only!
            QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(
                userResource,
                ec2::DummyHandler::instance(),
                &ec2::DummyHandler::onRequestDone );
        }
        return authResult;
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
    return !qnResPool->getResourceById<QnVideoWallResource>(QnUuid(login)).isNull();
}

QnAuthMethodRestrictionList* QnAuthHelper::restrictionList()
{
    return &m_authMethodRestrictionList;
}

QPair<QString, QString> QnAuthHelper::createAuthenticationQueryItemForPath( const QString& path, unsigned int periodMillis )
{
    QString authKey = QnUuid::createUuid().toString();
    if( authKey.isEmpty() )
        return QPair<QString, QString>();   //bad guid, failure
    authKey.replace( lit( "{" ), QString() );
    authKey.replace( lit("}"), QString() );

    //disabling authentication
    QMutexLocker lk( &m_mutex );

    //adding active period
    TimerManager::TimerGuard timerGuard(
        TimerManager::instance()->addTimer(
            std::bind(&QnAuthHelper::authenticationExpired, this, authKey, std::placeholders::_1),
            std::min(periodMillis, MAX_AUTHENTICATION_KEY_LIFE_TIME_MS ) ) );

    TempAuthenticationKeyCtx ctx;
    ctx.timeGuard = std::move( timerGuard );
    ctx.path = path;
    m_authenticatedPaths.emplace( authKey, std::move( ctx ) );

    return QPair<QString, QString>( TEMP_AUTH_KEY_NAME, authKey );
}

void QnAuthHelper::authenticationExpired( const QString& authKey, quint64 /*timerID*/ )
{
    QMutexLocker lk( &m_mutex );
    m_authenticatedPaths.erase( authKey );
}

QByteArray QnAuthHelper::createUserPasswordDigest(
    const QString& userName,
    const QString& password,
    const QString& realm )
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QString(lit("%1:%2:%3")).arg(userName, realm, password).toLatin1());
    return md5.result().toHex();
}

QByteArray QnAuthHelper::createHttpQueryAuthParam(
    const QString& userName,
    const QString& password,
    const QString& realm,
    const QByteArray& method,
    QByteArray nonce )
{
    //calculating user digest
    const QByteArray& ha1 = createUserPasswordDigest( userName, password, realm );

    //calculating "HA2"
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData( method );
    md5Hash.addData( ":" );
    const QByteArray nedoHa2 = md5Hash.result().toHex();

    //nonce
    if( nonce.isEmpty() )
        nonce = QByteArray::number( qnSyncTime->currentUSecsSinceEpoch(), 16 );

    //calculating auth digest
    md5Hash.reset();
    md5Hash.addData( ha1 );
    md5Hash.addData( ":" );
    md5Hash.addData( nonce );
    md5Hash.addData( ":" );
    md5Hash.addData( nedoHa2 );
    const QByteArray& authDigest = md5Hash.result().toHex();

    return (userName.toUtf8() + ":" + nonce + ":" + authDigest).toBase64();
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
QList<QByteArray> QnAuthHelper::smartSplit(const QByteArray& data, const char delimiter)
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

static QByteArray unquoteStr(const QByteArray& v)
{
    QByteArray value = v.trimmed();
    int pos1 = value.startsWith('\"') ? 1 : 0;
    int pos2 = value.endsWith('\"') ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

bool QnAuthHelper::doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, 
                                bool isProxy, QnUuid* authUserId, char delimiter, std::function<bool(const QByteArray&)> checkNonceFunc,
                                QnUserResourcePtr* const outUserResource)
{
    const QList<QByteArray>& authParams = smartSplit(authData, delimiter);

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
        QByteArray value = unquoteStr(data.mid(pos+1));
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

    QnUserResourcePtr userResource;
    //if (isNonceValid(nonce))
    if (checkNonceFunc(nonce))
    {
        QMutexLocker lock(&m_mutex);
        for(const QnUserResourcePtr& user: m_users)
        {
            if (user->getName().toUtf8().toLower() == userName)
            {
                if( outUserResource )
                    *outUserResource = user;

                userResource = user;
                QByteArray dbHash = user->getDigest();

                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(dbHash);
                md5Hash.addData(":");
                md5Hash.addData(nonce);
                md5Hash.addData(":");
                md5Hash.addData(ha2);
                QByteArray calcResponse = md5Hash.result().toHex();

                if (authUserId)
                    *authUserId = user->getId();

                if (calcResponse == response)
                    return true;
            }
        }

        // authenticate by media server auth_key
        for(const QnMediaServerResourcePtr& server: m_servers)
        {
            if (server->getId().toString().toUtf8().toLower() == userName)
            {
                QString ha1Data = lit("%1:%2:%3").arg(server->getId().toString()).arg(QnAppInfo::realm()).arg(server->getAuthKey());
                QCryptographicHash ha1( QCryptographicHash::Md5 );
                ha1.addData(ha1Data.toUtf8());

                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(ha1.result().toHex());
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

    if( userResource &&
        userResource->getRealm() != QnAppInfo::realm() )
    {
        //requesting client to re-calculate user's HA1 digest
        nx_http::insertOrReplaceHeader(
            &responseHeaders.headers,
            nx_http::HttpHeader( Qn::REALM_HEADER_NAME, QnAppInfo::realm().toLatin1() ) );
    }
    addAuthHeader(
        responseHeaders,
        userResource ? userResource->getRealm() : QnAppInfo::realm(),
        isProxy );

    return false;
}

bool QnAuthHelper::doBasicAuth(const QByteArray& authData, nx_http::Response& /*response*/, QnUuid* authUserId)
{
    QByteArray digest = QByteArray::fromBase64(authData);
    int pos = digest.indexOf(':');
    if (pos == -1)
        return false;
    QString userName = QUrl::fromPercentEncoding(digest.left(pos)).toLower();
    QString password = QUrl::fromPercentEncoding(digest.mid(pos+1));
     
    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toLower() == userName)
        {
            if (authUserId)
                *authUserId = user->getId();
            if (user->checkPassword(password))
            {
                if (user->getDigest().isEmpty())
                    emit emptyDigestDetected(user, userName, password);
                return true;
            }
        }
    }

    // authenticate by media server auth_key
    for(const QnMediaServerResourcePtr& server: m_servers)
    {
        if (server->getId().toString().toLower() == userName)
        {
            if (server->getAuthKey() == password)
                return true;
        }
    }

    return false;
}

bool QnAuthHelper::isCookieNonceValid(const QByteArray& nonce)
{
    if (nonce.isEmpty())
        return false;
    static const qint64 USEC_IN_SEC = 1000000ll;

    QMutexLocker lock(&m_cookieNonceCacheMutex);

    qint64 n1 = nonce.toLongLong(0, 16);
    qint64 n2 = qnSyncTime->currentUSecsSinceEpoch();

    bool rez;
    auto itr = m_cookieNonceCache.find(n1);
    if (itr == m_cookieNonceCache.end()) {
        m_cookieNonceCache.insert(n1, n2);
        rez = isNonceValid(nonce);
    }
    else {
        rez = n2 - itr.value() < COOKIE_EXPERATION_PERIOD * USEC_IN_SEC;
        itr.value() = n2;
    }

    // cleanup cookie cache

    qint64 minAllowedTime = n2 - COOKIE_EXPERATION_PERIOD * USEC_IN_SEC;
    for (auto itr = m_cookieNonceCache.begin(); itr != m_cookieNonceCache.end();)
    {
        if (itr.value() < minAllowedTime)
            itr = m_cookieNonceCache.erase(itr);
        else
            ++itr;
    }

    return rez;
}

bool QnAuthHelper::doCookieAuthorization(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId)
{
    nx_http::Response tmpHeaders;
    QnUserResourcePtr outUserResource;

    QMap<nx_http::BufferType, nx_http::BufferType> params;
    nx_http::header::parseDigestAuthParams( authData, &params, ';' );
        
    bool authResult = false;
    if( params.contains( URL_QUERY_AUTH_KEY_NAME ) )
    {
        //authenticating
        QnUuid userID;
        authResult = authenticateByUrl(
            QUrl::fromPercentEncoding(params.value(URL_QUERY_AUTH_KEY_NAME)).toUtf8(),
            method,
            &userID );
        outUserResource = m_users.value( userID );
        if( authUserId )
            *authUserId = userID;
    }
    else
    {
        authResult = doDigestAuth(
            method, authData, tmpHeaders, false, authUserId, ';',
            std::bind(&QnAuthHelper::isCookieNonceValid, this, std::placeholders::_1),
            &outUserResource);
    }
    if( !authResult )
    {
        nx_http::insertHeader(
            &responseHeaders.headers,
            nx_http::HttpHeader("Set-Cookie", lit("realm=%1; Path=/").arg(outUserResource ? outUserResource->getRealm() : QnAppInfo::realm()).toUtf8() ));

        QDateTime dt = qnSyncTime->currentDateTime().addSecs(COOKIE_EXPERATION_PERIOD);
        //QString nonce = lit("nonce=%1; Expires=%2; Path=/").arg(QLatin1String(getNonce())).arg(dateTimeToHTTPFormat(dt)); // Qt::RFC2822Date
        QString nonce = lit("nonce=%1; Path=/").arg(QLatin1String(getNonce()));
        nx_http::insertHeader(&responseHeaders.headers, nx_http::HttpHeader("Set-Cookie", nonce.toUtf8()));
        QString clientGuid = lit("%1=%2").arg(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME)).arg(QnUuid::createUuid().toString());
        nx_http::insertHeader(&responseHeaders.headers, nx_http::HttpHeader("Set-Cookie", clientGuid.toUtf8()));
    }
    return authResult;
}

void QnAuthHelper::addAuthHeader(
    nx_http::Response& response,
    const QString& realm,
    bool isProxy)
{
    QString auth(lit("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5"));
    //QString auth(lit("Digest realm=\"%1\",nonce=\"%2\",algorithm=MD5,qop=\"auth\""));
	QByteArray headerName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    nx_http::insertOrReplaceHeader(
        &response.headers,
        nx_http::HttpHeader(headerName, auth.arg(realm).arg(QLatin1String(getNonce())).toLatin1() ) );
}

bool QnAuthHelper::isNonceValid(const QByteArray& nonce) const
{
    {
        // nonce cache will help if time just changed
        QMutexLocker lock(&m_nonceMtx);
        if (m_digestNonceCache.contains(nonce) && m_digestNonceCache[nonce]->elapsed() < NONCE_TIMEOUT)
            return true;
    }
    // trust to unknown nonce to allow one step digest auth
    qint64 n1 = nonce.toLongLong(0, 16);
    qint64 n2 = qnSyncTime->currentUSecsSinceEpoch();
    return qAbs(n2 - n1) < NONCE_TIMEOUT;
}

QByteArray QnAuthHelper::getNonce()
{
    QByteArray result = QByteArray::number(qnSyncTime->currentUSecsSinceEpoch(), 16);
    QElapsedTimer* timeout = new QElapsedTimer();
    timeout->restart();
    QMutexLocker lock(&m_nonceMtx);
    m_digestNonceCache.insert(result, timeout);
    return result;
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

QByteArray QnAuthHelper::symmetricalEncode(const QByteArray& data)
{
    static const QByteArray mask = QByteArray::fromHex("4453D6654C634636990B2E5AA69A1312"); // generated from guid
    static const int maskSize = mask.size();
    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result.data()[i] ^= mask.data()[i % maskSize];
    return result;
}

bool QnAuthHelper::authenticateByUrl( const QByteArray& authRecordBase64, const QByteArray& method, QnUuid* authUserId ) const
{
    auto authRecord = QByteArray::fromBase64( authRecordBase64 );
    auto authFields = authRecord.split( ':' );
    if( authFields.size() != 3 )
        return false;

    const auto& userName = authFields[0];
    const auto& nonce = authFields[1];
    const auto& authDigest = authFields[2];

    if( !isNonceValid( nonce ) )
        return false;

    QMutexLocker lock( &m_mutex );
    for( const QnUserResourcePtr& user : m_users )
    {
        if( user->getName().toUtf8().toLower() != userName )
            continue;
        if (authUserId)
            *authUserId = user->getId();
        const QByteArray& ha1 = user->getDigest();

        QCryptographicHash md5Hash( QCryptographicHash::Md5 );
        md5Hash.addData( method );
        md5Hash.addData( ":" );
        const QByteArray nedoHa2 = md5Hash.result().toHex();

        md5Hash.reset();
        md5Hash.addData( ha1 );
        md5Hash.addData( ":" );
        md5Hash.addData( nonce );
        md5Hash.addData( ":" );
        md5Hash.addData( nedoHa2 );
        const QByteArray calcResponse = md5Hash.result().toHex();

        return calcResponse == authDigest;
    }

    return false;
}

QnUserResourcePtr QnAuthHelper::findUserByName( const QByteArray& nxUserName ) const
{
    QMutexLocker lock(&m_mutex);
    for( const QnUserResourcePtr& user: m_users )
    {
        if( user->getName().toUtf8().toLower() == nxUserName )
            return user;
    }
    return QnUserResourcePtr();
}
