/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cloud_user_authenticator.h"

#include <chrono>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <utils/common/app_info.h>
#include <utils/common/log.h>
#include <utils/common/sync_call.h>
#include <utils/network/http/auth_tools.h>

#include "cdb_nonce_fetcher.h"
#include "cloud/cloud_connection_manager.h"


static const std::chrono::minutes UNSUCCESSFUL_AUTHORIZATION_RESULT_CACHE_PERIOD(1);

CloudUserAuthenticator::CloudUserAuthenticator(
    std::unique_ptr<AbstractUserDataProvider> defaultAuthenticator,
    const CdbNonceFetcher& cdbNonceFetcher)
:
    m_defaultAuthenticator(std::move(defaultAuthenticator)),
    m_cdbNonceFetcher(cdbNonceFetcher)
{
    m_monotonicClock.restart();
}

QnResourcePtr CloudUserAuthenticator::findResByName(const QByteArray& nxUserName) const
{
    //NOTE this method returns NULL for cloud login. 
    //  This is OK, since NULL from this method does not signal authentication failure
    return m_defaultAuthenticator->findResByName(nxUserName);
}

Qn::AuthResult CloudUserAuthenticator::authorize(
    const QnResourcePtr& res,
    const nx_http::Method::ValueType& method,
    const nx_http::header::Authorization& authorizationHeader)
{
    QnResourcePtr authResource;
    Qn::AuthResult authResult = Qn::Auth_OK;
    std::tie(authResult, authResource) = authorize(method, authorizationHeader);
    if (authResult == Qn::Auth_OK)
    {
        return authResource == res
            ? Qn::Auth_OK
            : Qn::Auth_Forbidden;
    }

    return m_defaultAuthenticator->authorize(res, method, authorizationHeader);
}

std::tuple<Qn::AuthResult, QnResourcePtr> CloudUserAuthenticator::authorize(
    const nx_http::Method::ValueType& method,
    const nx_http::header::Authorization& authorizationHeader)
{
    if (!isValidCloudUserName(authorizationHeader.userid()) ||
        (authorizationHeader.authScheme != nx_http::header::AuthScheme::digest) ||      //supporting only digest authentication for cloud-based authentication
        (!m_cdbNonceFetcher.isValidCloudNonce(authorizationHeader.digest->params["nonce"])))    //nonce must be valid cloud nonce
    {
        return m_defaultAuthenticator->authorize(method, authorizationHeader);
    }

    const auto nonce = authorizationHeader.digest->params["nonce"];
    const auto ha2 = nx_http::calcHa2(method, authorizationHeader.digest->params["uri"]);

    NX_LOG(lm("Cloud authentication requested. username %1, nonce %2").
        arg(authorizationHeader.userid()).arg(nonce), cl_logDEBUG2);

    nx::String cloudNonce;
    nx::String nonceTrailer;
    if (!CdbNonceFetcher::parseCloudNonce(nonce, &cloudNonce, &nonceTrailer))
    {
        NX_LOG(lm("CloudUserAuthenticator. Bad nonce. username %1, nonce %2").
            arg(authorizationHeader.userid()).arg(nonce), cl_logDEBUG1);
        return m_defaultAuthenticator->authorize(method, authorizationHeader);
    }

    //checking authorization cache
    QnMutexLocker lk(&m_mutex);
    removeExpiredRecordsFromCache(&lk);
    auto cachedIter = m_authorizationCache.find(
        std::make_pair(authorizationHeader.userid(), cloudNonce));
    if (cachedIter == m_authorizationCache.end())
    {
        NX_LOG(lm("CloudUserAuthenticator. Auth data for username %1, nonce %2 not found in cache. Quering cloud...").
            arg(authorizationHeader.userid()).arg(nonce), cl_logDEBUG2);

        m_connection = CloudConnectionManager::instance()->getCloudConnection();
        nx::cdb::api::AuthRequest authRequest;
        authRequest.nonce = std::string(cloudNonce.constData(), cloudNonce.size());
        authRequest.realm = QnAppInfo::realm().toStdString();
        authRequest.username = authorizationHeader.userid();

        //TODO #ak MUST NOT invoke blocking call with mutex locked
        nx::cdb::api::ResultCode resultCode;
        nx::cdb::api::AuthResponse authResponse;
        std::tie(resultCode, authResponse) =
            makeSyncCall<nx::cdb::api::ResultCode, nx::cdb::api::AuthResponse>(
                std::bind(
                    &nx::cdb::api::AuthProvider::getAuthenticationResponse,
                    m_connection->authProvider(),
                    std::move(authRequest),
                    std::placeholders::_1));
        if (resultCode == nx::cdb::api::ResultCode::ok)
        {
            CloudAuthenticationData authData;
            authData.authorized = true;
            authData.data = std::move(authResponse);
            authData.expirationTime =
                m_monotonicClock.elapsed() +
                std::chrono::duration_cast<std::chrono::milliseconds>(authResponse.validPeriod).count();
            cachedIter = m_authorizationCache.emplace(
                std::make_pair(authorizationHeader.userid(), cloudNonce),
                std::move(authData)).first;
        }
        else
        {
            NX_LOG(lm("CloudUserAuthenticator. Failed to authenticate username %1, nonce %2 in cloud: %3").
                arg(authorizationHeader.userid()).arg(nonce).
                arg(CloudConnectionManager::instance()->connectionFactory().toString(resultCode)),
                cl_logDEBUG2);

            if (resultCode != nx::cdb::api::ResultCode::networkError &&
                resultCode != nx::cdb::api::ResultCode::unknownError)
            {
                //caching invalid authorization result
                CloudAuthenticationData authData;
                authData.authorized = false;
                authData.expirationTime =
                    m_monotonicClock.elapsed() +
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        UNSUCCESSFUL_AUTHORIZATION_RESULT_CACHE_PERIOD).count();
                cachedIter = m_authorizationCache.emplace(
                    std::make_pair(authorizationHeader.userid(), cloudNonce),
                    std::move(authData)).first;
            }
        }
    }

    if (cachedIter != m_authorizationCache.end())
    {
        if (!cachedIter->second.authorized)
            return std::make_tuple(Qn::Auth_WrongLogin, QnResourcePtr());
        
        //translating cloud account to local user
        auto localUser = getMappedLocalUserForCloudCredentials(
            authorizationHeader.userid(),
            cachedIter->second.data.accessRole);
        if (!localUser)
        {
            NX_LOG(lm("CloudUserAuthenticator. Failed to translate cloud user %1 to local user").
                arg(authorizationHeader.userid()), cl_logWARNING);
            return std::make_tuple(Qn::Auth_WrongLogin, QnResourcePtr());
        }

        const auto calculatedResponse = nx_http::calcResponseFromIntermediate(
            cachedIter->second.data.intermediateResponse.c_str(),
            cloudNonce.size(),
            nonceTrailer,
            ha2);
        if (authorizationHeader.digest->params["response"] == calculatedResponse)
        {
            //TODO #ak returning resolved user to HTTP client

            NX_LOG(lm("CloudUserAuthenticator. Successful cloud authentication. username %1, nonce %2").
                arg(authorizationHeader.userid()).arg(nonce), cl_logDEBUG2);
            return std::make_tuple(Qn::Auth_OK, std::move(localUser));
        }
        else
        {
            return std::make_tuple(Qn::Auth_WrongPassword, std::move(localUser));
        }
    }
    lk.unlock();

    return m_defaultAuthenticator->authorize(method, authorizationHeader);
}

bool CloudUserAuthenticator::isValidCloudUserName(const nx_http::StringType& userName) const
{
    //TODO #ak check for email
    return userName.indexOf('@') >= 0;
}

void CloudUserAuthenticator::removeExpiredRecordsFromCache(QnMutexLockerBase* const /*lk*/)
{
    const auto curClock = m_monotonicClock.elapsed();
    for (auto it = m_authorizationCache.begin();
         it != m_authorizationCache.end();
         )
    {
        if (it->second.expirationTime <= curClock)
            it = m_authorizationCache.erase(it);
        else
            ++it;
    }
}

QnUserResourcePtr CloudUserAuthenticator::getMappedLocalUserForCloudCredentials(
    const nx_http::StringType& userName,
    nx::cdb::api::SystemAccessRole cloudAccessRole) const
{
    const auto userNameQString = QString::fromUtf8(userName);
    //if there is user with same name in system, resolving to that user
    const auto res = qnResPool->getResourceByCond(
        [&userNameQString](const QnResourcePtr& res) {
            return (res.dynamicCast<QnUserResource>() != nullptr) &&
                   (res->getName() == userNameQString);
        });
    if (res)
        return res.staticCast<QnUserResource>();

    switch (cloudAccessRole)
    {
        case nx::cdb::api::SystemAccessRole::owner:
        case nx::cdb::api::SystemAccessRole::editorWithSharing:
        case nx::cdb::api::SystemAccessRole::editor:
            return qnResPool->getAdministrator();

        //TODO #ak resolve maintenance and viewer roles

        default:
            return QnUserResourcePtr();
    }
}
