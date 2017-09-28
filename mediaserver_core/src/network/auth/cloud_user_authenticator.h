/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <map>
#include <memory>
#include <set>

#include <QtCore/QElapsedTimer>

#include <nx/cloud/cdb/api/auth_provider.h>
#include <nx/cloud/cdb/api/connection.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/subscription.h>

#include "abstract_user_data_provider.h"

class CloudConnectionManager;
class CdbNonceFetcher;
class CloudUserInfoPool;

/** Add support for authentication using cloud account credentials. */
class CloudUserAuthenticator
:
    public AbstractUserDataProvider,
    public Qn::EnableSafeDirectConnection
{
public:
    /**
     * @param defaultAuthenticator Used to authenticate requests with local user credentials.
     */
    CloudUserAuthenticator(
        CloudConnectionManager* const cloudConnectionManager,
        std::unique_ptr<AbstractUserDataProvider> defaultAuthenticator,
        const CdbNonceFetcher& cdbNonceFetcher,
        const CloudUserInfoPool& cloudUserInfoPool);
    ~CloudUserAuthenticator();

    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const override;
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader,
        nx_http::HttpHeaders* const responseHeaders) override;
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader,
        nx_http::HttpHeaders* const responseHeaders) override;

    void clear();

private:
    struct CloudAuthenticationData
    {
    public:
        bool authorized;
        nx::cdb::api::AuthResponse data;
        qint64 expirationTime;

        CloudAuthenticationData()
        :
            authorized(false),
            expirationTime(0)
        {
        }
    };

    CloudConnectionManager* const m_cloudConnectionManager;
    std::unique_ptr<AbstractUserDataProvider> m_defaultAuthenticator;
    const CdbNonceFetcher& m_cdbNonceFetcher;
    const CloudUserInfoPool& m_cloudUserInfoPool;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
    /** map<pair<username, nonce>, auth_data> */
    std::map<
        std::pair<nx_http::StringType, nx_http::BufferType>,
        CloudAuthenticationData> m_authorizationCache;
    QElapsedTimer m_monotonicClock;
    /** set<pair<username, nonce>, auth_data> */
    std::set<std::pair<nx_http::StringType, nx_http::BufferType>> m_requestInProgress;

    void removeExpiredRecordsFromCache(QnMutexLockerBase* const lk);
    QnUserResourcePtr getMappedLocalUserForCloudCredentials(
        const nx_http::StringType& userName) const;
    void fetchAuthorizationFromCloud(
        QnMutexLockerBase* const lk,
        const nx_http::StringType& userid,
        const nx_http::StringType& cloudNonce);
    std::tuple<Qn::AuthResult, QnResourcePtr> authorizeWithCacheItem(
        QnMutexLockerBase* const lock,
        const CloudAuthenticationData& cacheItem,
        const nx_http::StringType& cloudNonce,
        const nx_http::StringType& nonceTrailer,
        nx_http::HttpHeaders* const responseHeaders,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) const;

    void cloudBindingStatusChanged(bool boundToCloud);
};
