#pragma once

#include <map>
#include <memory>
#include <set>

#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/subscription.h>

#include <nx/cloud/cdb/api/auth_provider.h>
#include <nx/cloud/cdb/api/connection.h>
#include <nx/vms/auth/abstract_user_data_provider.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace cloud_integration {

class CloudConnectionManager;
class CdbNonceFetcher;
class CloudUserInfoPool;

/**
 * Adds support for authentication using cloud account credentials.
 */
class CloudUserAuthenticator:
    public auth::AbstractUserDataProvider,
    public Qn::EnableSafeDirectConnection
{
public:
    /**
     * @param defaultAuthenticator Used to authenticate requests with local user credentials.
     */
    CloudUserAuthenticator(
        CloudConnectionManager* const cloudConnectionManager,
        std::unique_ptr<auth::AbstractUserDataProvider> defaultAuthenticator,
        const CdbNonceFetcher& cdbNonceFetcher,
        const CloudUserInfoPool& cloudUserInfoPool);
    ~CloudUserAuthenticator();

    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const override;
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx::network::http::Method::ValueType& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) override;
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx::network::http::Method::ValueType& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) override;

    void clear();

private:
    struct CloudAuthenticationData
    {
        bool authorized = false;
        nx::cdb::api::AuthResponse data;
        qint64 expirationTime = 0;
    };

    CloudConnectionManager* const m_cloudConnectionManager;
    std::unique_ptr<auth::AbstractUserDataProvider> m_defaultAuthenticator;
    const CdbNonceFetcher& m_cdbNonceFetcher;
    const CloudUserInfoPool& m_cloudUserInfoPool;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
    /** map<pair<username, nonce>, auth_data>. */
    std::map<
        std::pair<nx::network::http::StringType, nx::network::http::BufferType>,
        CloudAuthenticationData> m_authorizationCache;
    QElapsedTimer m_monotonicClock;
    /** set<pair<username, nonce>, auth_data>. */
    std::set<std::pair<nx::network::http::StringType, nx::network::http::BufferType>> m_requestInProgress;

    void removeExpiredRecordsFromCache(QnMutexLockerBase* const lk);
    QnUserResourcePtr getMappedLocalUserForCloudCredentials(
        const nx::network::http::StringType& userName) const;
    void fetchAuthorizationFromCloud(
        QnMutexLockerBase* const lk,
        const nx::network::http::StringType& userid,
        const nx::network::http::StringType& cloudNonce);
    std::tuple<Qn::AuthResult, QnResourcePtr> authorizeWithCacheItem(
        QnMutexLockerBase* const lock,
        const CloudAuthenticationData& cacheItem,
        const nx::network::http::StringType& cloudNonce,
        const nx::network::http::StringType& nonceTrailer,
        nx::network::http::HttpHeaders* const responseHeaders,
        const nx::network::http::Method::ValueType& method,
        const nx::network::http::header::Authorization& authorizationHeader) const;

    void cloudBindingStatusChanged(bool boundToCloud);
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
