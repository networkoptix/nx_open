/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CLOUD_USER_AUTHENTICATOR_H
#define NX_MS_CLOUD_USER_AUTHENTICATOR_H

#include <map>
#include <memory>
#include <set>

#include <QtCore/QElapsedTimer>

#include <cdb/auth_provider.h>
#include <cdb/connection.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <utils/common/safe_direct_connection.h>

#include "abstract_user_data_provider.h"


class CdbNonceFetcher;

//!Add support for authentication using cloud account credentials
class CloudUserAuthenticator
:
    public AbstractUserDataProvider,
    public Qn::EnableSafeDirectConnection
{
public:
    /*!
        \param defaultAuthenticator Used to authenticate requests with local user credentials
    */
    CloudUserAuthenticator(
        std::unique_ptr<AbstractUserDataProvider> defaultAuthenticator,
        const CdbNonceFetcher& cdbNonceFetcher);
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

    std::unique_ptr<AbstractUserDataProvider> m_defaultAuthenticator;
    const CdbNonceFetcher& m_cdbNonceFetcher;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
    //!map<pair<username, nonce>, auth_data>
    std::map<
        std::pair<nx_http::StringType, nx_http::BufferType>,
        CloudAuthenticationData> m_authorizationCache;
    QElapsedTimer m_monotonicClock;
    //!set<pair<username, nonce>, auth_data>
    std::set<std::pair<nx_http::StringType, nx_http::BufferType>> m_requestInProgress;

    bool isValidCloudUserName(const nx_http::StringType& userName) const;
    void removeExpiredRecordsFromCache(QnMutexLockerBase* const lk);
    QnUserResourcePtr getMappedLocalUserForCloudCredentials(
        const nx_http::StringType& userName,
        nx::cdb::api::SystemAccessRole cloudAccessRole) const;
    void fetchAuthorizationFromCloud(
        QnMutexLockerBase* const lk,
        const nx_http::StringType& userid,
        const nx_http::StringType& cloudNonce);
    std::tuple<Qn::AuthResult, QnResourcePtr> authorizeWithCacheItem(
        const CloudAuthenticationData& cacheItem,
        const nx_http::StringType& cloudNonce,
        const nx_http::StringType& nonceTrailer,
        nx_http::HttpHeaders* const responseHeaders,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) const;

private slots:
    void cloudBindingStatusChanged(bool bindedToCloud);
};

#endif  //NX_MS_CLOUD_USER_AUTHENTICATOR_H
