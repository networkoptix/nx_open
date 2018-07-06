#pragma once

#include "acti_resource.h"

#include <nx/utils/safe_direct_connection.h>
#include <utils/common/hash.h>
#include <nx/utils/url.h>

#ifdef ENABLE_ACTI

class QnActiSystemInfoChecker:
    public QObject,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    QnActiSystemInfoChecker(const nx::utils::Url &url);
    virtual ~QnActiSystemInfoChecker();
    boost::optional<QnActiResource::ActiSystemInfo> getSystemInfo();
    boost::optional<QAuthenticator> getSuccessfulAuth();
    bool isFailed();

    void setUrl(const nx::utils::Url &url);
    void setAuthOptions(QSet<QAuthenticator> authOptions);
    void setCacheExpirationInterval(const std::chrono::seconds& expirationInterval);

private:
    void initHttpClientIfNeededUnsafe();
    void startNewCycleIfNeededUnsafe();
    void tryToGetSystemInfoWithGivenAuthUnsafe(const QAuthenticator& auth);

    bool isLastCheckInCycleUnsafe();
    QAuthenticator getNextAuthToCheckUnsafe();

    void handleSystemInfoResponse(nx::network::http::AsyncHttpClientPtr httpClient);
    void handleFail();

private:
    nx::utils::Url m_baseUrl;

    boost::optional<QnActiResource::ActiSystemInfo> m_systemInfo;
    boost::optional<QAuthenticator> m_lastSuccessfulAuth;

    QSet<QAuthenticator> m_authOptions;
    QList<QAuthenticator> m_currentCycleAuthOptions;
    QAuthenticator m_currentAuth;

    QElapsedTimer m_cacheExpirationTimer;
    std::chrono::seconds m_cacheExpirationInterval;

    nx::network::http::AsyncHttpClientPtr m_httpClient;
    bool m_failed;
    bool m_cycleIsInProgress;
    bool m_terminated;

    mutable QnMutex m_mutex;
};

#endif // ENABLE_ACTI
