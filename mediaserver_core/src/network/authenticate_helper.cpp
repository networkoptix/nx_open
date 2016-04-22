
#include "authenticate_helper.h"

#include <QtCore/QCryptographicHash>

#include <utils/common/app_info.h>
#include <utils/common/cpp14.h>
#include <nx/utils/uuid.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include "utils/common/util.h"
#include "utils/common/synctime.h"
#include <nx/network/simple_http_client.h>
#include <utils/match/wildcard.h>
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "core/resource/media_server_resource.h"
#include "http/custom_headers.h"
#include "ldap/ldap_manager.h"
#include "network/authutil.h"
#include <nx_ec/dummy_handler.h>
#include <ldap/ldap_manager.h>
#include <nx/network/rtsp/rtsp_types.h>
#include "network/auth/time_based_nonce_provider.h"
#include "network/auth/generic_user_data_provider.h"
#include "network/auth/cdb_nonce_fetcher.h"
#include "network/auth/cloud_user_authenticator.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_user_manager.h>


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



static const qint64 LDAP_TIMEOUT = 1000000ll * 60 * 5;
static const QString COOKIE_DIGEST_AUTH( lit( "Authorization=Digest" ) );
static const QString TEMP_AUTH_KEY_NAME = lit( "authKey" );
static const nx_http::StringType URL_QUERY_AUTH_KEY_NAME = "auth";

const unsigned int QnAuthHelper::MAX_AUTHENTICATION_KEY_LIFE_TIME_MS = 60 * 60 * 1000;

QnAuthHelper::QnAuthHelper()
:
    m_nonceProvider(new CdbNonceFetcher(std::make_unique<TimeBasedNonceProvider>())),
    m_userDataProvider(new CloudUserAuthenticator(
        std::make_unique<GenericUserDataProvider>(),
        static_cast<const CdbNonceFetcher&>(*m_nonceProvider.get())))
{
#ifndef USE_USER_RESOURCE_PROVIDER
    connect(qnResPool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceChanged(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
#endif
}

QnAuthHelper::~QnAuthHelper()
{
#ifndef USE_USER_RESOURCE_PROVIDER
    disconnect(qnResPool, NULL, this, NULL);
#endif
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
                response,
                authUserId);
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
        int customAuthInfoPos = cookie.indexOf(URL_QUERY_AUTH_KEY_NAME);
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
        //TODO #ak better call m_userDataProvider->authorize here
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

        Qn::AuthResult authResult = Qn::Auth_Forbidden;
        if (authorizationHeader.authScheme == nx_http::header::AuthScheme::digest)
        {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::httpDigest;

            authResult = doDigestAuth(
                request.requestLine.method, authorizationHeader, response, isProxy, authUserId);
        }
        else if (authorizationHeader.authScheme == nx_http::header::AuthScheme::basic) {
            if (usedAuthMethod)
                *usedAuthMethod = AuthMethod::httpBasic;
            authResult = doBasicAuth(request.requestLine.method, authorizationHeader, response, authUserId);
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
        else if( userResource && userResource->isLdap() )
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
    nx::utils::TimerManager::TimerGuard timerGuard(
        nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnAuthHelper::authenticationExpired, this, authKey, std::placeholders::_1),
            std::chrono::milliseconds(std::min(periodMillis, MAX_AUTHENTICATION_KEY_LIFE_TIME_MS))));

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

Qn::AuthResult QnAuthHelper::doDigestAuth(
    const QByteArray& method,
    const nx_http::header::Authorization& authorization,
    nx_http::Response& responseHeaders,
    bool isProxy,
    QnUuid* authUserId,
    QnUserResourcePtr* const outUserResource)
{
    const QByteArray userName = authorization.digest->userid;
    const QByteArray response = authorization.digest->params["response"];
    const QByteArray nonce = authorization.digest->params["nonce"];
    const QByteArray realm = authorization.digest->params["realm"];
    const QByteArray uri = authorization.digest->params["uri"];

    if (nonce.isEmpty() || userName.isEmpty() || realm.isEmpty())
        return Qn::Auth_WrongDigest;

#ifndef USE_USER_RESOURCE_PROVIDER
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(method);
    md5Hash.addData(":");
    md5Hash.addData(uri);
    QByteArray ha2 = md5Hash.result().toHex();
#endif

    QnUserResourcePtr userResource;
    Qn::AuthResult errCode = Qn::Auth_WrongDigest;
    if (m_nonceProvider->isNonceValid(nonce))
    {
#ifdef USE_USER_RESOURCE_PROVIDER
        errCode = Qn::Auth_WrongLogin;

        QnResourcePtr res;
        std::tie(errCode, res) = m_userDataProvider->authorize(
            method,
            authorization,
            &responseHeaders.headers);
        bool tryOnceAgain = false;
        if (userResource = res.dynamicCast<QnUserResource>())
        {
            if (outUserResource)
                *outUserResource = userResource;
            if (userResource->passwordExpired())
            {
                //user password has expired, validating password
                errCode = doPasswordProlongation(userResource);
                if (errCode != Qn::Auth_OK)
                    return errCode;
                //have to call m_userDataProvider->authorize once again with password prolonged
                tryOnceAgain = true;
            }
            if (authUserId)
                *authUserId = userResource->getId();
        }

        if (tryOnceAgain)
            errCode = m_userDataProvider->authorize(
                res,
                method,
                authorization,
                &responseHeaders.headers);
        if (errCode == Qn::Auth_OK)
            return Qn::Auth_OK;
#else
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
#endif
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

Qn::AuthResult QnAuthHelper::doBasicAuth(
    const QByteArray& method,
    const nx_http::header::Authorization& authorization,
    nx_http::Response& response,
    QnUuid* authUserId)
{
    NX_ASSERT(authorization.authScheme == nx_http::header::AuthScheme::basic);

    Qn::AuthResult errCode = Qn::Auth_WrongLogin;

#ifdef USE_USER_RESOURCE_PROVIDER
    QnResourcePtr res;
    std::tie(errCode, res) = m_userDataProvider->authorize(
        method,
        authorization,
        &response.headers);
    bool tryOnceAgain = false;
    if (auto user = res.dynamicCast<QnUserResource>())
    {
        if (authUserId)
            *authUserId = user->getId();
        if (user->passwordExpired())
        {
            //user password has expired, validating password
            errCode = doPasswordProlongation(user);
            if (errCode != Qn::Auth_OK)
                return errCode;
            tryOnceAgain = true;
        }
    }

    if (tryOnceAgain)
        errCode = m_userDataProvider->authorize(
            res,
            method,
            authorization,
            &response.headers);
    if (errCode == Qn::Auth_OK)
    {
        if (auto user = res.dynamicCast<QnUserResource>())
        {
            if (user->getDigest().isEmpty())
                emit emptyDigestDetected(user, authorization.basic->userid, authorization.basic->password);
        }
        return Qn::Auth_OK;
    }
#else
    for(const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toLower() == authorization.basic->userid)
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

            if (user->checkPassword(authorization.basic->password))
            {
                if (user->getDigest().isEmpty())
                    emit emptyDigestDetected(user, authorization.basic->userid, authorization.basic->password);
                return Qn::Auth_OK;
            }
        }
    }

    // authenticate by media server auth_key
    for(const QnMediaServerResourcePtr& server: m_servers)
    {
        if (server->getId().toString().toLower() == authorization.basic->userid)
        {
            if (server->getAuthKey() == authorization.basic->password)
                return Qn::Auth_OK;
        }
    }
#endif

    return errCode;
}

Qn::AuthResult QnAuthHelper::doCookieAuthorization(
    const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId)
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
            responseHeaders,
            &userID,
            &outUserResource);
        if( authUserId )
            *authUserId = userID;
    }
    else
    {
        nx_http::header::Authorization authorization(nx_http::header::AuthScheme::digest);
        authorization.digest->parse(authData, ';');
        authResult = doDigestAuth(
            method, authorization, tmpHeaders, false, authUserId, &outUserResource);
    }
    if( authResult != Qn::Auth_OK)
    {
#if 0
        nx_http::insertHeader(
            &responseHeaders.headers,
            nx_http::HttpHeader("Set-Cookie", lit("realm=%1; Path=/").arg(outUserResource ? outUserResource->getRealm() : QnAppInfo::realm()).toUtf8() ));

        QString nonce = lit("nonce=%1; Path=/").arg(QLatin1String(m_nonceProvider->generateNonce()));
        nx_http::insertHeader(&responseHeaders.headers, nx_http::HttpHeader("Set-Cookie", nonce.toUtf8()));
        QString clientGuid = lit("%1=%2").arg(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME)).arg(QnUuid::createUuid().toString());
        nx_http::insertHeader(&responseHeaders.headers, nx_http::HttpHeader("Set-Cookie", clientGuid.toUtf8()));
#endif        
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
        auth.arg(realm).arg(QLatin1String(m_nonceProvider->generateNonce())).toLatin1() ) );
}

QByteArray QnAuthHelper::generateNonce() const
{
    return m_nonceProvider->generateNonce();
}

#ifndef USE_USER_RESOURCE_PROVIDER
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
#endif

QByteArray QnAuthHelper::symmetricalEncode(const QByteArray& data)
{
    static const QByteArray mask = QByteArray::fromHex("4453D6654C634636990B2E5AA69A1312"); // generated from guid
    static const int maskSize = mask.size();
    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result.data()[i] ^= mask.data()[i % maskSize];
    return result;
}

Qn::AuthResult QnAuthHelper::authenticateByUrl(
    const QByteArray& authRecordBase64,
    const QByteArray& method,
    nx_http::Response& response,
    QnUuid* authUserId,
    QnUserResourcePtr* const outUserResource) const
{
    auto authRecord = QByteArray::fromBase64( authRecordBase64 );
    auto authFields = authRecord.split( ':' );
    if( authFields.size() != 3 )
        return Qn::Auth_WrongDigest;

    nx_http::header::Authorization authorization(nx_http::header::AuthScheme::digest);
    authorization.digest->userid = authFields[0];
    authorization.digest->params["response"] = authFields[2];
    authorization.digest->params["nonce"] = authFields[1];
    authorization.digest->params["realm"] = QnAppInfo::realm().toUtf8();
    //digestAuthParams.params["uri"];   uri is empty

    if( !m_nonceProvider->isNonceValid(authorization.digest->params["nonce"]) )
        return Qn::Auth_WrongDigest;

#ifdef USE_USER_RESOURCE_PROVIDER
    QnResourcePtr res;
    Qn::AuthResult errCode = Qn::Auth_WrongLogin;
    std::tie(errCode, res) = m_userDataProvider->authorize(
        method,
        authorization,
        &response.headers);
    if (!res)
        return Qn::Auth_WrongLogin;

    if (auto user = res.dynamicCast<QnUserResource>())
    {
        if (authUserId)
            *authUserId = user->getId();
    }

    if (errCode == Qn::Auth_OK)
    {
        if (auto user = res.dynamicCast<QnUserResource>())
            if (outUserResource)
                *outUserResource = user;
    }
    return errCode;
#else
    QnMutexLocker lock( &m_mutex );
    Qn::AuthResult errCode = Qn::Auth_WrongLogin;
    for( const QnUserResourcePtr& user : m_users )
    {
        if( user->getName().toUtf8().toLower() != authorization.digest->userid)
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
        md5Hash.addData(authorization.digest->params["nonce"] );
        md5Hash.addData( ":" );
        md5Hash.addData( nedoHa2 );
        const QByteArray calcResponse = md5Hash.result().toHex();

        if (calcResponse == authorization.digest->params["response"])
            return Qn::Auth_OK;
    }

    return errCode;
#endif
}

QnUserResourcePtr QnAuthHelper::findUserByName( const QByteArray& nxUserName ) const
{
#ifdef USE_USER_RESOURCE_PROVIDER
    auto res = m_userDataProvider->findResByName(nxUserName);
    if (auto user = res.dynamicCast<QnUserResource>())
        return user;
#else
    QnMutexLocker lock(&m_mutex);
    for( const QnUserResourcePtr& user: m_users )
    {
        if( user->getName().toUtf8().toLower() == nxUserName )
            return user;
    }
#endif
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

    ec2::ApiUserData userData;
    fromResourceToApi(userResource, userData);


    QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(
        userData,
        QString(),
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
