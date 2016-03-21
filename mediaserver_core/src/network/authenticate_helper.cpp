
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
#include "ldap/ldap_manager.h"
#include "network/authutil.h"
#include <nx_ec/dummy_handler.h>
#include <ldap/ldap_manager.h>
#include <utils/network/rtsp/rtsp_types.h>


////////////////////////////////////////////////////////////
//// class QnAuthHelper
////////////////////////////////////////////////////////////

void QnAuthHelper::UserDigestData::parse( const nx_http::Request& request )
{
    ha1Digest = nx_http::getHeaderValue( request.headers, Qn::HA1_DIGEST_HEADER_NAME );
    realm = nx_http::getHeaderValue( request.headers, Qn::REALM_HEADER_NAME );
    cryptSha512Hash = nx_http::getHeaderValue( request.headers, Qn::CRYPT_SHA512_HASH_HEADER_NAME );
    nxUserName = nx_http::getHeaderValue( request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME );
}

bool QnAuthHelper::UserDigestData::empty() const
{
    return ha1Digest.isEmpty() || realm.isEmpty() || nxUserName.isEmpty() || cryptSha512Hash.isEmpty();
}



QnAuthHelper* QnAuthHelper::m_instance;

static const qint64 NONCE_TIMEOUT = 1000000ll * 60 * 5;
static const qint64 LDAP_TIMEOUT = 1000000ll * 60 * 5;
static const qint64 COOKIE_EXPIRATION_PERIOD = 3600;
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

Qn::AuthResult QnAuthHelper::authenticate(const nx_http::Request& request, nx_http::Response& response, bool isProxy, QnUuid* authUserId, AuthMethod::Value* usedAuthMethod)
{
    if (authUserId)
        *authUserId = QnUuid();
    if (usedAuthMethod)
        *usedAuthMethod = AuthMethod::noAuth;

    const QUrlQuery urlQuery( request.requestLine.url.query() );

    const unsigned int allowedAuthMethods = m_authMethodRestrictionList.getAllowedAuthMethods( request );
    if( allowedAuthMethods == 0 )
        return Qn::Auth_Forbidden;   //NOTE assert?

    if( allowedAuthMethods & AuthMethod::noAuth )
        return Qn::Auth_OK;

    {
        QnMutexLocker lk( &m_mutex );
        if( urlQuery.hasQueryItem( TEMP_AUTH_KEY_NAME ) )
        {
            auto it = m_authenticatedPaths.find( urlQuery.queryItemValue( TEMP_AUTH_KEY_NAME ) );
            if( it != m_authenticatedPaths.end() &&
                it->second.path == request.requestLine.url.path() )
            {
                if (usedAuthMethod)
                    *usedAuthMethod = AuthMethod::tempUrlQueryParam;
                return Qn::Auth_OK;
            }
        }
    }

    if( allowedAuthMethods & AuthMethod::videowall )
    {
        const nx_http::StringType& videoWall_auth = nx_http::getHeaderValue( request.headers, Qn::VIDEOWALL_GUID_HEADER_NAME );
        if (!videoWall_auth.isEmpty()) {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::videowall;
            if (qnResPool->getResourceById<QnVideoWallResource>(QnUuid(videoWall_auth)).isNull())
                return Qn::Auth_Forbidden;
            else
                return Qn::Auth_OK;
        }
    }

    if( allowedAuthMethods & AuthMethod::urlQueryParam )
    {
        const QByteArray& authQueryParam = urlQuery.queryItemValue(
            isProxy ? lit( "proxy_auth" ) : QString::fromLatin1(URL_QUERY_AUTH_KEY_NAME) ).toLatin1();
        if( !authQueryParam.isEmpty() )
        {
            auto authResult = authenticateByUrl(
                authQueryParam,
                request.requestLine.version.protocol == nx_rtsp::rtsp_1_0.protocol
                    ? "PLAY"    //for rtsp always using PLAY since client software does not know 
                                //which request underlying player will issue first
                    : request.requestLine.method,
                authUserId,
                std::bind(&QnAuthHelper::isNonceValid, this, std::placeholders::_1));
            if(authResult == Qn::Auth_OK)
            {
                if (usedAuthMethod)
                    *usedAuthMethod = AuthMethod::urlQueryParam;
                return Qn::Auth_OK;
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
            Qn::AuthResult authResult = Qn::Auth_WrongDigest;
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::httpDigest;
            QnUserResourcePtr userResource;
            if( !nxUserName.isEmpty() )
            {
                userResource = findUserByName( nxUserName );
                if( userResource )
                {
                    QString desiredRealm = QnAppInfo::realm();
                    if (userResource->isLdap()) {
                        auto errCode = QnLdapManager::instance()->realm(&desiredRealm);
                        if (errCode != Qn::Auth_OK)
                            return errCode;
                    }
                    if( userResource->getRealm() != desiredRealm ||
                        userResource->getDigest().isEmpty() )   //in case of ldap digest is initially empty
                    {
                        //requesting client to re-calculate digest after upgrade to 2.4
                        nx_http::insertOrReplaceHeader(
                            &response.headers,
                            nx_http::HttpHeader( Qn::REALM_HEADER_NAME, desiredRealm.toLatin1() ) );
                        if (!userResource->isLdap())
                        {
                            addAuthHeader(
                                response,
                                userResource,
                                isProxy,
                                false); //requesting Basic authorization
                            return authResult;
                        }
                    }
                    else if( userResource->isLdap() &&
                        userResource->passwordExpired())
                    {
                        authResult = doPasswordProlongation(userResource);
                        if (authResult != Qn::Auth_OK) {
                            //requesting password update from client
                            nx_http::insertOrReplaceHeader(
                                &response.headers,
                                nx_http::HttpHeader( Qn::REALM_HEADER_NAME, desiredRealm.toLatin1() ) );
                        }
                    }
                }
            }
            else {
                // use admin's realm by default for better compatibility with previous version
                userResource = qnResPool->getAdministrator();
            }

            addAuthHeader(
                response,
                userResource,
                isProxy);
            return authResult;
        }

        nx_http::header::Authorization authorizationHeader;
        if( !authorizationHeader.parse( authorization ) )
            return Qn::Auth_Forbidden;
        QnUserResourcePtr userResource = findUserByName( authorizationHeader.userid() );

        QString desiredRealm = QnAppInfo::realm();
        if (userResource && userResource->isLdap()) {
            Qn::AuthResult authResult = QnLdapManager::instance()->realm(&desiredRealm);
            if (authResult != Qn::Auth_OK)
                return authResult;
        }

        UserDigestData userDigestData;
        userDigestData.parse( request );

        if( userResource )
        {
            if (!userResource->isEnabled())
                return Qn::Auth_WrongLogin;

            if( userResource->isLdap() && !userDigestData.empty() )
            {
                //this block is supposed to be executed after changing password on LDAP server
                //checking for digest in received request

                //checking received credentials for validity
                if( checkDigestValidity(userResource, userDigestData.ha1Digest ) == Qn::Auth_OK)
                {
                    //changing stored user's password
                    applyClientCalculatedPasswordHashToResource( userResource, userDigestData );
                }
            }

            if (userResource->getDigest().isEmpty() && userResource->isLdap())
            {
                //requesting client to calculate users's digest
                nx_http::insertOrReplaceHeader(
                    &response.headers,
                    nx_http::HttpHeader( Qn::REALM_HEADER_NAME, desiredRealm.toLatin1() ) );

                return Qn::Auth_WrongDigest;   //user has no password yet
            }
        }

        nx_http::StringType authType;
        nx_http::StringType authData;
        int pos = authorization.indexOf(L' ');
        if (pos > 0) {
            authType = authorization.left(pos).toLower();
            authData = authorization.mid(pos+1);
        }

        Qn::AuthResult authResult = Qn::Auth_Forbidden;
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
            authResult = Qn::Auth_Forbidden;
        }
        if( authResult  == Qn::Auth_OK)
        {
            //checking whether client re-calculated ha1 digest
            if( userDigestData.empty() )
                return authResult;

            if( !userResource || (userResource->getRealm() == QString::fromUtf8(userDigestData.realm)) )
                return authResult;
            //saving new user's digest
            applyClientCalculatedPasswordHashToResource( userResource, userDigestData );
        }
        else if(  userResource && userResource->isLdap() )
        {
            //password has been changed in active directory? Requesting new digest...
            nx_http::insertOrReplaceHeader(
                &response.headers,
                nx_http::HttpHeader( Qn::REALM_HEADER_NAME, desiredRealm.toLatin1() ) );
        }
        return authResult;
    }

    return Qn::Auth_Forbidden;   //failed to authorise request with any method
}

Qn::AuthResult QnAuthHelper::authenticate( const QString& login, const QByteArray& digest ) const
{
    QnMutexLocker lock( &m_mutex );
    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toLower() == login.toLower()) {
            if (user->getDigest() == digest)
                return Qn::Auth_OK;
            else
                return Qn::Auth_Forbidden;
        }
    }
    //checking if it videowall connect
    if (qnResPool->getResourceById<QnVideoWallResource>(QnUuid(login)).isNull())
        return Qn::Auth_Forbidden;
    else
        return Qn::Auth_OK;
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
    QnMutexLocker lk( &m_mutex );

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
    QnMutexLocker lk( &m_mutex );
    m_authenticatedPaths.erase( authKey );
}

Qn::AuthResult QnAuthHelper::doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, 
                                bool isProxy, QnUuid* authUserId, char delimiter, std::function<bool(const QByteArray&)> checkNonceFunc,
                                QnUserResourcePtr* const outUserResource)
{
    const QMap<QByteArray, QByteArray> authParams = parseAuthData(authData, delimiter);
    if (authParams.isEmpty())
        return Qn::Auth_Forbidden;

    QByteArray userName;
    QByteArray response;
    QByteArray nonce;
    QByteArray realm;
    QByteArray uri;

    for (QByteArray key : authParams.keys())
    {
        QByteArray value = authParams[key];

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

    if (nonce.isEmpty() || userName.isEmpty() || realm.isEmpty())
        return Qn::Auth_WrongDigest;

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(method);
    md5Hash.addData(":");
    md5Hash.addData(uri);
    QByteArray ha2 = md5Hash.result().toHex();

    QnUserResourcePtr userResource;
    Qn::AuthResult errCode = Qn::Auth_WrongDigest;
    if (checkNonceFunc(nonce))
    {
        errCode = Qn::Auth_WrongLogin;
        userResource = findUserByName(userName);
        if( userResource )
        {
            errCode = Qn::Auth_WrongPassword;
            if( outUserResource )
                *outUserResource = userResource;

            if( userResource->passwordExpired() )
            {
                //user password has expired, validating password
                errCode = doPasswordProlongation(userResource);
                if( errCode != Qn::Auth_OK )
                    return errCode;
            }

            QByteArray dbHash = userResource->getDigest();

            QCryptographicHash md5Hash( QCryptographicHash::Md5 );
            md5Hash.addData(dbHash);
            md5Hash.addData(":");
            md5Hash.addData(nonce);
            md5Hash.addData(":");
            md5Hash.addData(ha2);
            QByteArray calcResponse = md5Hash.result().toHex();

            if( authUserId )
                *authUserId = userResource->getId();
            if (calcResponse == response)
                return Qn::Auth_OK;
        }

        QnMutexLocker lock( &m_mutex );

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
                    return Qn::Auth_OK;
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
        userResource,
        isProxy );

    if (errCode == Qn::Auth_WrongLogin && !QUuid(userName).isNull())
        errCode = Qn::Auth_WrongInternalLogin;

    return errCode;
}

Qn::AuthResult QnAuthHelper::doBasicAuth(const QByteArray& authData, nx_http::Response& /*response*/, QnUuid* authUserId)
{
    QByteArray digest = QByteArray::fromBase64(authData);
    int pos = digest.indexOf(':');
    if (pos == -1)
        return Qn::Auth_Forbidden;
    QString userName = QUrl::fromPercentEncoding(digest.left(pos)).toLower();
    QString password = QUrl::fromPercentEncoding(digest.mid(pos+1));
    Qn::AuthResult errCode = Qn::Auth_WrongLogin;     
    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toLower() == userName)
        {
            errCode = Qn::Auth_WrongPassword;
            if (authUserId)
                *authUserId = user->getId();

            if (!user->isEnabled())
                continue;

            if( user->passwordExpired() )
            {
                //user password has expired, validating password
                auto authResult = doPasswordProlongation(user);
                if( authResult != Qn::Auth_OK)
                    return authResult;
            }

            if (user->checkPassword(password))
            {
                if (user->getDigest().isEmpty())
                    emit emptyDigestDetected(user, userName, password);
                return Qn::Auth_OK;
            }
        }
    }

    // authenticate by media server auth_key
    for(const QnMediaServerResourcePtr& server: m_servers)
    {
        if (server->getId().toString().toLower() == userName)
        {
            if (server->getAuthKey() == password)
                return Qn::Auth_OK;
        }
    }

    return errCode;
}

QByteArray QnAuthHelper::getNonce()
{
    QnMutexLocker lock(&m_cookieNonceCacheMutex);
    const qint64 nonce = qnSyncTime->currentUSecsSinceEpoch();
    m_cookieNonceCache.insert(nonce, nonce);
    return QByteArray::number(nonce, 16);
}

bool QnAuthHelper::isNonceValid(const QByteArray& nonce)
{
    if (nonce.isEmpty())
        return false;
    static const qint64 USEC_IN_SEC = 1000000ll;

    QnMutexLocker lock( &m_cookieNonceCacheMutex );

    const qint64 intNonce = nonce.toLongLong(0, 16);
    const qint64 curTimeUSec = qnSyncTime->currentUSecsSinceEpoch();

    bool rez;
    auto itr = m_cookieNonceCache.find(intNonce);
    if (itr == m_cookieNonceCache.end()) {
        rez = qAbs(curTimeUSec - intNonce) < NONCE_TIMEOUT;
        if (rez)
            m_cookieNonceCache.insert(intNonce, curTimeUSec);
    }
    else {
        rez = curTimeUSec - itr.value() < COOKIE_EXPIRATION_PERIOD * USEC_IN_SEC;
        itr.value() = curTimeUSec;
    }

    // cleanup cookie cache

    const qint64 minAllowedTime = curTimeUSec - COOKIE_EXPIRATION_PERIOD * USEC_IN_SEC;
    for (auto itr = m_cookieNonceCache.begin(); itr != m_cookieNonceCache.end();)
    {
        if (itr.value() < minAllowedTime)
            itr = m_cookieNonceCache.erase(itr);
        else
            ++itr;
    }

    return rez;
}

Qn::AuthResult QnAuthHelper::doCookieAuthorization(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId)
{
    nx_http::Response tmpHeaders;
    QnUserResourcePtr outUserResource;

    QMap<nx_http::BufferType, nx_http::BufferType> params;
    nx_http::header::parseDigestAuthParams( authData, &params, ';' );
        
    Qn::AuthResult authResult = Qn::Auth_Forbidden;
    if( params.contains( URL_QUERY_AUTH_KEY_NAME ) )
    {
        //authenticating
        QnUuid userID;
        authResult = authenticateByUrl(
            QUrl::fromPercentEncoding(params.value(URL_QUERY_AUTH_KEY_NAME)).toUtf8(),
            method,
            &userID,
            std::bind(&QnAuthHelper::isNonceValid, this, std::placeholders::_1));
        outUserResource = m_users.value( userID );
        if( authUserId )
            *authUserId = userID;
    }
    else
    {
        authResult = doDigestAuth(
            method, authData, tmpHeaders, false, authUserId, ';',
            std::bind(&QnAuthHelper::isNonceValid, this, std::placeholders::_1),
            &outUserResource);
    }
    if( authResult != Qn::Auth_OK)
    {
        nx_http::insertHeader(
            &responseHeaders.headers,
            nx_http::HttpHeader("Set-Cookie", lit("realm=%1; Path=/").arg(outUserResource ? outUserResource->getRealm() : QnAppInfo::realm()).toUtf8() ));

        QDateTime dt = qnSyncTime->currentDateTime().addSecs(COOKIE_EXPIRATION_PERIOD);
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
    const QnUserResourcePtr& userResource,
    bool isProxy,
    bool isDigest)
{
    QString realm;
    if( userResource ) {
        if (userResource->isLdap())
            QnLdapManager::instance()->realm(&realm);
        else
            realm = userResource->getRealm();
    }
    else
        realm = QnAppInfo::realm();

    const QString auth(
        isDigest
            ? lit("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5")
            : lit("Basic realm=\"%1\""));
    //QString auth(lit("Digest realm=\"%1\",nonce=\"%2\",algorithm=MD5,qop=\"auth\""));
    const QByteArray headerName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    nx_http::insertOrReplaceHeader( &response.headers, nx_http::HttpHeader(
        headerName,
        auth.arg(realm).arg(QLatin1String(getNonce())).toLatin1() ) );
}

void QnAuthHelper::at_resourcePool_resourceAdded(const QnResourcePtr & res)
{
    QnMutexLocker lock( &m_mutex );

    QnUserResourcePtr user = res.dynamicCast<QnUserResource>();
    QnMediaServerResourcePtr server = res.dynamicCast<QnMediaServerResource>();
    if (user)
        m_users.insert(user->getId(), user);
    else if (server)
        m_servers.insert(server->getId(), server);
}

void QnAuthHelper::at_resourcePool_resourceRemoved(const QnResourcePtr &res)
{
    QnMutexLocker lock( &m_mutex );

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

Qn::AuthResult QnAuthHelper::authenticateByUrl( const QByteArray& authRecordBase64, const QByteArray& method, QnUuid* authUserId,
                                           std::function<bool(const QByteArray&)> checkNonceFunc) const
{
    auto authRecord = QByteArray::fromBase64( authRecordBase64 );
    auto authFields = authRecord.split( ':' );
    if( authFields.size() != 3 )
        return Qn::Auth_WrongDigest;

    const auto& userName = authFields[0];
    const auto& nonce = authFields[1];
    const auto& authDigest = authFields[2];

    if( !checkNonceFunc( nonce ) )
        return Qn::Auth_WrongDigest;
    
    QnMutexLocker lock( &m_mutex );
    Qn::AuthResult errCode = Qn::Auth_WrongLogin;
    for( const QnUserResourcePtr& user : m_users )
    {
        if( user->getName().toUtf8().toLower() != userName )
            continue;
        errCode = Qn::Auth_WrongPassword;
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

        if (calcResponse == authDigest)
            return Qn::Auth_OK;
    }

    return errCode;
}

QnUserResourcePtr QnAuthHelper::findUserByName( const QByteArray& nxUserName ) const
{
    QnMutexLocker lock(&m_mutex);
    for( const QnUserResourcePtr& user: m_users )
    {
        if( user->getName().toUtf8().toLower() == nxUserName )
            return user;
    }
    return QnUserResourcePtr();
}

void QnAuthHelper::applyClientCalculatedPasswordHashToResource(
    const QnUserResourcePtr& userResource,
    const QnAuthHelper::UserDigestData& userDigestData )
{
    //TODO #ak set following properties atomically
    userResource->setRealm( QString::fromUtf8( userDigestData.realm ) );
    userResource->setDigest( userDigestData.ha1Digest, true );
    userResource->setCryptSha512Hash( userDigestData.cryptSha512Hash );
    userResource->setHash( QByteArray() );
    QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(
        userResource,
        ec2::DummyHandler::instance(),
        &ec2::DummyHandler::onRequestDone );
}

Qn::AuthResult QnAuthHelper::doPasswordProlongation(QnUserResourcePtr userResource)
{
    if( !userResource->isLdap() )
        return Qn::Auth_OK;

    QString name = userResource->getName();
    QString digest = userResource->getDigest();

    auto errorCode = QnLdapManager::instance()->authenticateWithDigest( name, digest );
    if( errorCode != Qn::Auth_OK ) {
        if (!userResource->passwordExpired())
            return Qn::Auth_OK;
        else
            return errorCode;
    }

    if( userResource->getName() != name || userResource->getDigest() != digest )  //user data has been updated somehow while performing ldap request
        return userResource->passwordExpirationTimestamp() > qnSyncTime->currentMSecsSinceEpoch() ? Qn::Auth_OK : Qn::Auth_PasswordExpired;
    userResource->prolongatePassword();
    return Qn::Auth_OK;
}

Qn::AuthResult QnAuthHelper::checkDigestValidity(QnUserResourcePtr userResource, const QByteArray& digest )
{
    if( !userResource->isLdap() )
        return Qn::Auth_OK;

    return QnLdapManager::instance()->authenticateWithDigest( userResource->getName(), QLatin1String(digest) );
}
