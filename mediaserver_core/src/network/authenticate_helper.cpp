
#include "authenticate_helper.h"

#include <QtCore/QCryptographicHash>

#include <utils/crypt/symmetrical.h>
#include <utils/common/app_info.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include "utils/common/util.h"
#include "utils/common/synctime.h"
#include <nx/network/simple_http_client.h>
#include <nx/utils/match/wildcard.h>
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "core/resource/media_server_resource.h"
#include <nx/network/http/custom_headers.h>
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
#include <nx/network/http/auth_tools.h>
#include <nx/utils/string.h>

#include "cloud/cloud_manager_group.h"

////////////////////////////////////////////////////////////
//// class QnAuthHelper
////////////////////////////////////////////////////////////

void QnAuthHelper::UserDigestData::parse(const nx_http::Request& request)
{
    ha1Digest = nx_http::getHeaderValue(request.headers, Qn::HA1_DIGEST_HEADER_NAME);
    realm = nx_http::getHeaderValue(request.headers, Qn::REALM_HEADER_NAME);
    cryptSha512Hash = nx_http::getHeaderValue(request.headers, Qn::CRYPT_SHA512_HASH_HEADER_NAME);
    nxUserName = nx_http::getHeaderValue(request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME);
}

bool QnAuthHelper::UserDigestData::empty() const
{
    return ha1Digest.isEmpty() || realm.isEmpty() || nxUserName.isEmpty() || cryptSha512Hash.isEmpty();
}



static const qint64 LDAP_TIMEOUT = 1000000ll * 60 * 5;
static const QString COOKIE_DIGEST_AUTH(lit("Authorization=Digest"));
static const QString TEMP_AUTH_KEY_NAME = lit("authKey");

const unsigned int QnAuthHelper::MAX_AUTHENTICATION_KEY_LIFE_TIME_MS = 60 * 60 * 1000;

QnAuthHelper::QnAuthHelper(
    QnCommonModule* commonModule,
    TimeBasedNonceProvider* timeBasedNonceProvider,
    CloudManagerGroup* cloudManagerGroup)
:
    QnCommonModuleAware(commonModule),
    m_timeBasedNonceProvider(timeBasedNonceProvider),
    m_nonceProvider(&cloudManagerGroup->authenticationNonceFetcher),
    m_userDataProvider(&cloudManagerGroup->userAuthenticator)
{
#ifndef USE_USER_RESOURCE_PROVIDER
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)), this, SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceChanged(const QnResourcePtr &)), this, SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this, SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
#endif
}

QnAuthHelper::~QnAuthHelper()
{
#ifndef USE_USER_RESOURCE_PROVIDER
    disconnect(resourcePool(), NULL, this, NULL);
#endif
}

Qn::AuthResult QnAuthHelper::authenticate(
    const nx_http::Request& request,
    nx_http::Response& response,
    bool isProxy,
    Qn::UserAccessData* accessRights,
    nx_http::AuthMethod::Value* usedAuthMethod)
{
    if (accessRights)
        *accessRights = Qn::UserAccessData();
    if (usedAuthMethod)
        *usedAuthMethod = nx_http::AuthMethod::noAuth;

    const QUrlQuery urlQuery(request.requestLine.url.query());

    const unsigned int allowedAuthMethods = m_authMethodRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods == 0)
        return Qn::Auth_Forbidden;   //NOTE assert?

    if (allowedAuthMethods & nx_http::AuthMethod::noAuth)
        return Qn::Auth_OK;

    {
        QnMutexLocker lk(&m_mutex);
        if (urlQuery.hasQueryItem(TEMP_AUTH_KEY_NAME))
        {
            auto it = m_authenticatedPaths.find(urlQuery.queryItemValue(TEMP_AUTH_KEY_NAME));
            if (it != m_authenticatedPaths.end() &&
                it->second.path == request.requestLine.url.path())
            {
                if (usedAuthMethod)
                    *usedAuthMethod = nx_http::AuthMethod::tempUrlQueryParam;
                if (accessRights)
                    *accessRights = it->second.accessRights;
                return Qn::Auth_OK;
            }
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::videowall)
    {
        const nx_http::StringType& videoWall_auth = nx_http::getHeaderValue(request.headers, Qn::VIDEOWALL_GUID_HEADER_NAME);
        if (!videoWall_auth.isEmpty()) {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::videowall;
            if (resourcePool()->getResourceById<QnVideoWallResource>(QnUuid(videoWall_auth)).isNull())
                return Qn::Auth_Forbidden;
            else
            {
                if (accessRights)
                    *accessRights = Qn::kVideowallUserAccess;
                return Qn::Auth_OK;
            }
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::urlQueryParam)
    {
        const QByteArray& authQueryParam = urlQuery.queryItemValue(
            isProxy ? lit("proxy_auth") : QString::fromLatin1(Qn::URL_QUERY_AUTH_KEY_NAME)).toLatin1();
        if (!authQueryParam.isEmpty())
        {
            auto authResult = authenticateByUrl(
                authQueryParam,
                request.requestLine.version.protocol == nx_rtsp::rtsp_1_0.protocol
                ? "PLAY"    //for rtsp always using PLAY since client software does not know
                            //which request underlying player will issue first
                : request.requestLine.method,
                response,
                accessRights);
            if (authResult == Qn::Auth_OK)
            {
                if (usedAuthMethod)
                    *usedAuthMethod = nx_http::AuthMethod::urlQueryParam;
                return Qn::Auth_OK;
            }
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::cookie)
    {
        const QString& cookie = QLatin1String(nx_http::getHeaderValue(request.headers, "Cookie"));
        int customAuthInfoPos = cookie.indexOf(Qn::URL_QUERY_AUTH_KEY_NAME);
        if (customAuthInfoPos >= 0) {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::cookie;
            return doCookieAuthorization("GET", cookie.toUtf8(), response, accessRights);
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::http)
    {
        const nx_http::StringType& authorization = isProxy
            ? nx_http::getHeaderValue(request.headers, "Proxy-Authorization")
            : nx_http::getHeaderValue(request.headers, "Authorization");
        const nx_http::StringType nxUserName = nx_http::getHeaderValue(request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME);
        bool canUpdateRealm = request.headers.find(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME) != request.headers.end();
        if (authorization.isEmpty())
        {
            Qn::AuthResult authResult = Qn::Auth_WrongDigest;
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpDigest;
            QnUserResourcePtr userResource;
            if (!nxUserName.isEmpty())
            {
                userResource = findUserByName(nxUserName);
                if (userResource)
                {
                    QString desiredRealm = nx::network::AppInfo::realm();
                    if (userResource->isLdap()) {
                        auto errCode = QnLdapManager::instance()->realm(&desiredRealm);
                        if (errCode != Qn::Auth_OK)
                            return errCode;
                    }
                    if (canUpdateRealm &&
                        (userResource->getRealm() != desiredRealm ||
                            userResource->getDigest().isEmpty()))   //in case of ldap digest is initially empty
                    {
                        //requesting client to re-calculate digest after upgrade to 2.4
                        nx_http::insertOrReplaceHeader(
                            &response.headers,
                            nx_http::HttpHeader(Qn::REALM_HEADER_NAME, desiredRealm.toLatin1()));
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
                }
            }
            else {
                // use admin's realm by default for better compatibility with previous version
                // in case of default realm upgrade
                userResource = resourcePool()->getAdministrator();
            }

            addAuthHeader(
                response,
                userResource,
                isProxy);
            return authResult;
        }

        nx_http::header::Authorization authorizationHeader;
        if (!authorizationHeader.parse(authorization))
            return Qn::Auth_Forbidden;
        //TODO #ak better call m_userDataProvider->authorize here
        QnUserResourcePtr userResource = findUserByName(authorizationHeader.userid());

        QString desiredRealm = nx::network::AppInfo::realm();
        if (userResource && userResource->isLdap()) {
            Qn::AuthResult authResult = QnLdapManager::instance()->realm(&desiredRealm);
            if (authResult != Qn::Auth_OK)
                return authResult;
        }

        UserDigestData userDigestData;
        userDigestData.parse(request);

        if (userResource)
        {
            if (!userResource->isEnabled())
                return Qn::Auth_WrongLogin;

            if (userResource->isLdap() &&
                (userResource->getDigest().isEmpty() || userResource->getRealm() != desiredRealm))
            {
                //checking received credentials for validity
                Qn::AuthResult authResult = checkDigestValidity(userResource, userDigestData.ha1Digest);
                if (authResult != Qn::Auth_OK)
                    return authResult;
                //changing stored user's password
                applyClientCalculatedPasswordHashToResource(userResource, userDigestData);
                userResource->prolongatePassword();
            }
        }

        Qn::AuthResult authResult = Qn::Auth_Forbidden;
        if (authorizationHeader.authScheme == nx_http::header::AuthScheme::digest)
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpDigest;

            authResult = doDigestAuth(
                request.requestLine.method, authorizationHeader, response, isProxy, accessRights);
        }
        else if (authorizationHeader.authScheme == nx_http::header::AuthScheme::basic)
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpBasic;
            authResult = doBasicAuth(request.requestLine.method, authorizationHeader, response, accessRights);
        }
        else {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpBasic;
            authResult = Qn::Auth_Forbidden;
        }

        if (authResult == Qn::Auth_OK)
        {

            // update user information if authorization by server authKey and user-name is specified
            if (accessRights &&
                resourcePool()->getResourceById<QnMediaServerResource>(accessRights->userId))
            {
                *accessRights = Qn::kSystemAccess;
                auto itr = request.headers.find(Qn::CUSTOM_USERNAME_HEADER_NAME);
                if (itr != request.headers.end())
                {
                    auto userRes = findUserByName(itr->second);
                    if (userRes)
                        *accessRights = Qn::UserAccessData(userRes->getId());
                }
            }


            //checking whether client re-calculated ha1 digest
            if (userDigestData.empty())
                return authResult;

            if (!userResource || (userResource->getRealm() == QString::fromUtf8(userDigestData.realm)))
                return authResult;
            //saving new user's digest
            applyClientCalculatedPasswordHashToResource(userResource, userDigestData);
        }
        else if (userResource && userResource->isLdap())
        {
            //password has been changed in active directory? Requesting new digest...
            nx_http::insertOrReplaceHeader(
                &response.headers,
                nx_http::HttpHeader(Qn::REALM_HEADER_NAME, desiredRealm.toLatin1()));
        }
        return authResult;
    }

    return Qn::Auth_Forbidden;   //failed to authorise request with any method
}

nx_http::AuthMethodRestrictionList* QnAuthHelper::restrictionList()
{
    return &m_authMethodRestrictionList;
}

QPair<QString, QString> QnAuthHelper::createAuthenticationQueryItemForPath(
    const Qn::UserAccessData& accessRights,
    const QString& path,
    unsigned int periodMillis)
{
    QString authKey = QnUuid::createUuid().toString();
    if (authKey.isEmpty())
        return QPair<QString, QString>();   //bad guid, failure
    authKey.replace(lit("{"), QString());
    authKey.replace(lit("}"), QString());

    //disabling authentication
    QnMutexLocker lk(&m_mutex);

    //adding active period
    nx::utils::TimerManager::TimerGuard timerGuard(
        nx::utils::TimerManager::instance(),
        nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnAuthHelper::authenticationExpired, this, authKey, std::placeholders::_1),
            std::chrono::milliseconds(std::min(periodMillis, MAX_AUTHENTICATION_KEY_LIFE_TIME_MS))));

    TempAuthenticationKeyCtx ctx;
    ctx.timeGuard = std::move(timerGuard);
    ctx.path = path;
    ctx.accessRights = accessRights;
    m_authenticatedPaths.emplace(authKey, std::move(ctx));

    return QPair<QString, QString>(TEMP_AUTH_KEY_NAME, authKey);
}

void QnAuthHelper::authenticationExpired(const QString& authKey, quint64 /*timerID*/)
{
    QnMutexLocker lk(&m_mutex);
    m_authenticatedPaths.erase(authKey);
}

Qn::AuthResult QnAuthHelper::doDigestAuth(
    const QByteArray& method,
    const nx_http::header::Authorization& authorization,
    nx_http::Response& responseHeaders,
    bool isProxy,
    Qn::UserAccessData* accessRights)
{
    const QByteArray userName = authorization.digest->userid;
    const QByteArray response = authorization.digest->params["response"];
    const QByteArray nonce = authorization.digest->params["nonce"];
    const QByteArray realm = authorization.digest->params["realm"];
    const QByteArray uri = authorization.digest->params["uri"];

    if (nonce.isEmpty() || userName.isEmpty() || realm.isEmpty())
        return Qn::Auth_WrongDigest;

#ifndef USE_USER_RESOURCE_PROVIDER
    QCryptographicHash md5Hash(QCryptographicHash::Md5);
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

        if (res && accessRights)
            *accessRights = Qn::UserAccessData(res->getId());

        bool tryOnceAgain = false;
        if (userResource = res.dynamicCast<QnUserResource>())
        {
            if (userResource->passwordExpired())
            {
                //user password has expired, validating password
                errCode = doPasswordProlongation(userResource);
                if (errCode != Qn::Auth_OK)
                    return errCode;
                //have to call m_userDataProvider->authorize once again with password prolonged
                tryOnceAgain = true;
            }
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
        if (userResource)
        {
            errCode = Qn::Auth_WrongPassword;

            if (userResource->passwordExpired())
            {
                //user password has expired, validating password
                errCode = doPasswordProlongation(userResource);
                if (errCode != Qn::Auth_OK)
                    return errCode;
            }

            QByteArray dbHash = userResource->getDigest();

            QCryptographicHash md5Hash(QCryptographicHash::Md5);
            md5Hash.addData(dbHash);
            md5Hash.addData(":");
            md5Hash.addData(nonce);
            md5Hash.addData(":");
            md5Hash.addData(ha2);
            QByteArray calcResponse = md5Hash.result().toHex();

            if (authUserId)
                *authUserId = userResource->getId();
            if (calcResponse == response)
                return Qn::Auth_OK;
        }

        QnMutexLocker lock(&m_mutex);

        // authenticate by media server auth_key
        for (const QnMediaServerResourcePtr& server : m_servers)
        {
            if (server->getId().toString().toUtf8().toLower() == userName)
            {
                QString ha1Data = lit("%1:%2:%3").arg(server->getId().toString()).arg(nx::network::AppInfo::realm()).arg(server->getAuthKey());
                QCryptographicHash ha1(QCryptographicHash::Md5);
                ha1.addData(ha1Data.toUtf8());

                QCryptographicHash md5Hash(QCryptographicHash::Md5);
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

    if (userResource &&
        userResource->getRealm() != nx::network::AppInfo::realm())
    {
        //requesting client to re-calculate user's HA1 digest
        nx_http::insertOrReplaceHeader(
            &responseHeaders.headers,
            nx_http::HttpHeader(Qn::REALM_HEADER_NAME, nx::network::AppInfo::realm().toLatin1()));
    }
    addAuthHeader(
        responseHeaders,
        userResource,
        isProxy);

    if (errCode == Qn::Auth_WrongLogin && !QUuid(userName).isNull())
        errCode = Qn::Auth_WrongInternalLogin;

    return errCode;
}

Qn::AuthResult QnAuthHelper::doBasicAuth(
    const QByteArray& method,
    const nx_http::header::Authorization& authorization,
    nx_http::Response& response,
    Qn::UserAccessData* accessRights)
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
        if (accessRights)
            *accessRights = Qn::UserAccessData(user->getId());
        if (user->passwordExpired())
        {
            //user password has expired, validating password
            errCode = doPasswordProlongation(user);
            if (errCode != Qn::Auth_OK)
                return errCode;
            tryOnceAgain = true;
        }
    }
    else if (auto server = res.dynamicCast<QnMediaServerResource>())
    {
        if (accessRights)
            *accessRights = Qn::UserAccessData(server->getId());
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
    for (const QnUserResourcePtr& user : m_users)
    {
        if (user->getName().toLower() == authorization.basic->userid)
        {
            errCode = Qn::Auth_WrongPassword;
            if (authUserId)
                *authUserId = user->getId();

            if (!user->isEnabled())
                continue;

            if (user->passwordExpired())
            {
                //user password has expired, validating password
                auto authResult = doPasswordProlongation(user);
                if (authResult != Qn::Auth_OK)
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
    for (const QnMediaServerResourcePtr& server : m_servers)
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
    const QByteArray& method,
    const QByteArray& authData,
    nx_http::Response& responseHeaders,
    Qn::UserAccessData* accessRights)
{
    nx_http::Response tmpHeaders;

    QMap<nx_http::BufferType, nx_http::BufferType> params;
    nx::utils::parseNameValuePairs(authData, ';', &params);

    Qn::AuthResult authResult = Qn::Auth_Forbidden;
    if (params.contains(Qn::URL_QUERY_AUTH_KEY_NAME))
    {
        //authenticating
        authResult = authenticateByUrl(
            QUrl::fromPercentEncoding(params.value(Qn::URL_QUERY_AUTH_KEY_NAME)).toUtf8(),
            method,
            responseHeaders,
            accessRights);
    }
    else
    {
        nx_http::header::Authorization authorization(nx_http::header::AuthScheme::digest);
        authorization.digest->parse(authData, ';');
        authResult = doDigestAuth(
            method, authorization, tmpHeaders, false, accessRights);
    }
    if (authResult != Qn::Auth_OK)
    {
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
    if (userResource)
    {
        if (userResource->isLdap())
            QnLdapManager::instance()->realm(&realm);
        else
            realm = userResource->getRealm();
    }
    else
    {
        realm = nx::network::AppInfo::realm();
    }

    const QString auth =
        isDigest
        ? lit("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5")
            .arg(realm)
            .arg(QLatin1String(m_nonceProvider->generateNonce()))
        : lit("Basic realm=\"%1\"").arg(realm);

    //QString auth(lit("Digest realm=\"%1\",nonce=\"%2\",algorithm=MD5,qop=\"auth\""));
    const QByteArray headerName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    nx_http::insertOrReplaceHeader(&response.headers, nx_http::HttpHeader(
        headerName,
        auth.toLatin1()));
}

QByteArray QnAuthHelper::generateNonce(NonceProvider provider) const
{
    if (provider == NonceProvider::automatic)
        return m_nonceProvider->generateNonce();
    else
        return m_timeBasedNonceProvider->generateNonce();
}

#ifndef USE_USER_RESOURCE_PROVIDER
void QnAuthHelper::at_resourcePool_resourceAdded(const QnResourcePtr & res)
{
    QnMutexLocker lock(&m_mutex);

    QnUserResourcePtr user = res.dynamicCast<QnUserResource>();
    QnMediaServerResourcePtr server = res.dynamicCast<QnMediaServerResource>();
    if (user)
        m_users.insert(user->getId(), user);
    else if (server)
        m_servers.insert(server->getId(), server);
}

void QnAuthHelper::at_resourcePool_resourceRemoved(const QnResourcePtr &res)
{
    QnMutexLocker lock(&m_mutex);

    m_users.remove(res->getId());
    m_servers.remove(res->getId());
}
#endif

QByteArray QnAuthHelper::symmetricalEncode(const QByteArray& data)
{
    return nx::utils::encodeSimple(data);
}

Qn::AuthResult QnAuthHelper::authenticateByUrl(
    const QByteArray& authRecordBase64,
    const QByteArray& method,
    nx_http::Response& response,
    Qn::UserAccessData* accessRights) const
{
    auto authRecord = QByteArray::fromBase64(authRecordBase64);
    auto authFields = authRecord.split(':');
    if (authFields.size() != 3)
        return Qn::Auth_WrongDigest;

    nx_http::header::Authorization authorization(nx_http::header::AuthScheme::digest);
    authorization.digest->userid = authFields[0];
    authorization.digest->params["response"] = authFields[2];
    authorization.digest->params["nonce"] = authFields[1];
    authorization.digest->params["realm"] = nx::network::AppInfo::realm().toUtf8();
    //digestAuthParams.params["uri"];   uri is empty

    if (!m_nonceProvider->isNonceValid(authorization.digest->params["nonce"]))
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
        if (accessRights)
            *accessRights = Qn::UserAccessData(user->getId());
    }

    return errCode;
#else
    QnMutexLocker lock(&m_mutex);
    Qn::AuthResult errCode = Qn::Auth_WrongLogin;
    for (const QnUserResourcePtr& user : m_users)
    {
        if (user->getName().toUtf8().toLower() != authorization.digest->userid)
            continue;
        errCode = Qn::Auth_WrongPassword;
        if (authUserId)
            *authUserId = user->getId();
        const QByteArray& ha1 = user->getDigest();

        QCryptographicHash md5Hash(QCryptographicHash::Md5);
        md5Hash.addData(method);
        md5Hash.addData(":");
        const QByteArray nedoHa2 = md5Hash.result().toHex();

        md5Hash.reset();
        md5Hash.addData(ha1);
        md5Hash.addData(":");
        md5Hash.addData(authorization.digest->params["nonce"]);
        md5Hash.addData(":");
        md5Hash.addData(nedoHa2);
        const QByteArray calcResponse = md5Hash.result().toHex();

        if (calcResponse == authorization.digest->params["response"])
            return Qn::Auth_OK;
    }

    return errCode;
#endif
}

QnUserResourcePtr QnAuthHelper::findUserByName(const QByteArray& nxUserName) const
{
#ifdef USE_USER_RESOURCE_PROVIDER
    auto res = m_userDataProvider->findResByName(nxUserName);
    if (auto user = res.dynamicCast<QnUserResource>())
        return user;
#else
    QnMutexLocker lock(&m_mutex);
    for (const QnUserResourcePtr& user : m_users)
    {
        if (user->getName().toUtf8().toLower() == nxUserName)
            return user;
    }
#endif
    return QnUserResourcePtr();
}

void QnAuthHelper::applyClientCalculatedPasswordHashToResource(
    const QnUserResourcePtr& userResource,
    const QnAuthHelper::UserDigestData& userDigestData)
{
    //TODO #ak set following properties atomically
    userResource->setRealm(QString::fromUtf8(userDigestData.realm));
    userResource->setDigest(userDigestData.ha1Digest, true);
    userResource->setCryptSha512Hash(userDigestData.cryptSha512Hash);
    userResource->setHash(QByteArray());

    ec2::ApiUserData userData;
    fromResourceToApi(userResource, userData);


    commonModule()->ec2Connection()->getUserManager(Qn::kSystemAccess)->save(
        userData,
        QString(),
        ec2::DummyHandler::instance(),
        &ec2::DummyHandler::onRequestDone);
}

Qn::AuthResult QnAuthHelper::doPasswordProlongation(QnUserResourcePtr userResource)
{
    if (!userResource->isLdap())
        return Qn::Auth_OK;

    QString name = userResource->getName();
    QString digest = userResource->getDigest();

    auto errorCode = QnLdapManager::instance()->authenticateWithDigest(name, digest);
    if (errorCode != Qn::Auth_OK) {
        if (!userResource->passwordExpired())
            return Qn::Auth_OK;
        else
            return errorCode;
    }

    if (userResource->getName() != name || userResource->getDigest() != digest)  //user data has been updated somehow while performing ldap request
        return userResource->passwordExpirationTimestamp() > qnSyncTime->currentMSecsSinceEpoch() ? Qn::Auth_OK : Qn::Auth_PasswordExpired;
    userResource->prolongatePassword();
    return Qn::Auth_OK;
}

Qn::AuthResult QnAuthHelper::checkDigestValidity(QnUserResourcePtr userResource, const QByteArray& digest)
{
    if (!userResource->isLdap())
        return Qn::Auth_OK;

    return QnLdapManager::instance()->authenticateWithDigest(userResource->getName(), QLatin1String(digest));
}

bool QnAuthHelper::checkUserPassword(const QnUserResourcePtr& user, const QString& password)
{
    if (!user->isCloud())
        return user->checkLocalUserPassword(password);

    // 3. Cloud users
    QByteArray auth = createHttpQueryAuthParam(
        user->getName(),
        password,
        user->getRealm(),
        "GET",
        qnAuthHelper->generateNonce());

    nx_http::Response response;
    return authenticateByUrl(auth, QByteArray("GET"), response) == Qn::Auth_OK;
}
