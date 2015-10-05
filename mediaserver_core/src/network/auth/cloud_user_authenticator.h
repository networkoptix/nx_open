/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CLOUD_USER_AUTHENTICATOR_H
#define NX_MS_CLOUD_USER_AUTHENTICATOR_H

#include <map>
#include <memory>

#include <QtCore/QElapsedTimer>

#include <cdb/auth_provider.h>
#include <cdb/connection.h>
#include <utils/thread/mutex.h>

#include "abstract_user_data_provider.h"


class CdbNonceFetcher;

//!Add support for authentication using cloud account credentials
class CloudUserAuthenticator
:
    public AbstractUserDataProvider
{
public:
    /*!
        \param defaultAuthenticator Used to authenticate requests with local user credentials
    */
    CloudUserAuthenticator(
        std::unique_ptr<AbstractUserDataProvider> defaultAuthenticator,
        const CdbNonceFetcher& cdbNonceFetcher);

    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const override;
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) override;
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader) override;

private:
    struct CloudAuthenticationData
    {
    public:
        nx::cdb::api::AuthResponse data;
        qint64 expirationTime;

        CloudAuthenticationData()
        :
            expirationTime(0)
        {
        }
    };

    std::unique_ptr<AbstractUserDataProvider> m_defaultAuthenticator;
    const CdbNonceFetcher& m_cdbNonceFetcher;
    mutable QnMutex m_mutex;
    //!map<pair<username, nonce>, authd_ata>
    std::map<
        std::pair<nx_http::StringType, nx_http::BufferType>,
        CloudAuthenticationData> m_authorizationCache;
    QElapsedTimer m_monotonicClock;
    std::unique_ptr<nx::cdb::api::Connection> m_connection;

    bool isValidCloudUserName(const nx_http::StringType& userName) const;
    void removeExpiredRecordsFromCache(QnMutexLockerBase* const lk);
};

#endif  //NX_MS_CLOUD_USER_AUTHENTICATOR_H
