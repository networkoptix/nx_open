#ifdef ENABLE_ACTI

#include "acti_system_info_checker.h"

namespace {

const std::chrono::seconds kDefaultCacheExpirationTime(300); //< 5 minutes
const QString kActiUserParameterName = lit("USER");
const QString kActiPasswordParamterName = lit("PWD");
const QString kActiSystemInfoParameterName = lit("SYSTEM_INFO");

const std::chrono::milliseconds kActiSystemInfoRequestSendTimeout(4000);
const std::chrono::milliseconds kActiSystemInfoResponseReceiveTimeout(4000);

} // namespace


QnActiSystemInfoChecker::QnActiSystemInfoChecker(const nx::utils::Url& url):
    m_baseUrl(url),
    m_systemInfo(boost::none),
    m_lastSuccessfulAuth(boost::none),
    m_cacheExpirationInterval(kDefaultCacheExpirationTime),
    m_failed(false),
    m_cycleIsInProgress(false),
    m_terminated(false)
{
    m_cacheExpirationTimer.invalidate();
}

QnActiSystemInfoChecker::~QnActiSystemInfoChecker()
{
    directDisconnectAll();
    nx::network::http::AsyncHttpClientPtr httpClientCopy;

    {
        QnMutexLocker lock(&m_mutex);
        if (m_httpClient)
            httpClientCopy = m_httpClient;
    }

    httpClientCopy->pleaseStopSync();
}

boost::optional<QnActiResource::ActiSystemInfo> QnActiSystemInfoChecker::getSystemInfo()
{
    QnMutexLocker lock(&m_mutex);
    startNewCycleIfNeededUnsafe();
    return m_systemInfo;
}

boost::optional<QAuthenticator> QnActiSystemInfoChecker::getSuccessfulAuth()
{
    QnMutexLocker lock(&m_mutex);
    startNewCycleIfNeededUnsafe();
    return m_lastSuccessfulAuth;
}

bool QnActiSystemInfoChecker::isFailed()
{
    QnMutexLocker lock(&m_mutex);
    startNewCycleIfNeededUnsafe();
    return m_failed;
}

void QnActiSystemInfoChecker::setUrl(const nx::utils::Url& url)
{
    QnMutexLocker lock(&m_mutex);
    m_baseUrl = url;
}

void QnActiSystemInfoChecker::setAuthOptions(QSet<QAuthenticator> authOptions)
{
    QnMutexLocker lock(&m_mutex);
    m_authOptions.unite(authOptions);
}

void QnActiSystemInfoChecker::setCacheExpirationInterval(const std::chrono::seconds& expirationInterval)
{
    QnMutexLocker lock(&m_mutex);
    m_cacheExpirationInterval = expirationInterval;
}

void QnActiSystemInfoChecker::initHttpClientIfNeededUnsafe()
{
    if (m_httpClient)
        return;

    m_httpClient = nx::network::http::AsyncHttpClient::create();
    m_httpClient->setSendTimeoutMs(kActiSystemInfoRequestSendTimeout.count());
    m_httpClient->setResponseReadTimeoutMs(kActiSystemInfoResponseReceiveTimeout.count());

    directConnect(
        m_httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &QnActiSystemInfoChecker::handleSystemInfoResponse);
}

void QnActiSystemInfoChecker::startNewCycleIfNeededUnsafe()
{
    auto expirationInterval = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_cacheExpirationInterval);

    bool needToStartNewCycle =
            (!m_cacheExpirationTimer.isValid() || m_cacheExpirationTimer.elapsed() > expirationInterval.count())
            && !m_cycleIsInProgress;

    if(!needToStartNewCycle)
        return;

    m_currentCycleAuthOptions.clear();
    if (m_lastSuccessfulAuth)
        m_currentCycleAuthOptions.push_back(*m_lastSuccessfulAuth);

    for (const auto& authOption: m_authOptions)
    {
        if (!m_lastSuccessfulAuth || authOption != *m_lastSuccessfulAuth)
            m_currentCycleAuthOptions.push_back(authOption);
    }

    if (!m_currentCycleAuthOptions.isEmpty())
    {
        m_cycleIsInProgress = true;
        tryToGetSystemInfoWithGivenAuthUnsafe(getNextAuthToCheckUnsafe());
    }
}

void QnActiSystemInfoChecker::tryToGetSystemInfoWithGivenAuthUnsafe(const QAuthenticator& auth)
{
    initHttpClientIfNeededUnsafe();

    nx::utils::Url requestUrl = m_baseUrl;
    requestUrl.setPath(lit("/cgi-bin/system"));

    QUrlQuery query;
    query.addQueryItem(kActiUserParameterName, auth.user());
    query.addQueryItem(kActiPasswordParamterName, auth.password());
    query.addQueryItem(kActiSystemInfoParameterName, QString());

    requestUrl.setQuery(query);

    m_currentAuth = auth;

    m_httpClient->doGet(requestUrl);
}

bool QnActiSystemInfoChecker::isLastCheckInCycleUnsafe()
{
    return m_currentCycleAuthOptions.isEmpty();
}

QAuthenticator QnActiSystemInfoChecker::getNextAuthToCheckUnsafe()
{
    QAuthenticator auth;
    if (!m_currentCycleAuthOptions.isEmpty())
    {
        auth = m_currentCycleAuthOptions.front();
        m_currentCycleAuthOptions.pop_front();
    }

    return auth;
}

void QnActiSystemInfoChecker::handleSystemInfoResponse(nx::network::http::AsyncHttpClientPtr httpClient)
{
    if (httpClient->state() != nx::network::http::AsyncClient::State::sDone)
    {
        handleFail();
        return;
    }

    auto response = httpClient->response();
    if (!response || response->statusLine.statusCode != nx::network::http::StatusCode::ok)
    {
        handleFail();
        return;
    }

    auto&& systemInfo = QnActiResource::parseSystemInfo(httpClient->fetchMessageBodyBuffer());
    if(systemInfo.isEmpty())
    {
        handleFail();
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);
        m_cycleIsInProgress = false;
        m_failed = false;
        m_lastSuccessfulAuth = m_currentAuth;
        m_systemInfo = systemInfo;
        m_cacheExpirationTimer.restart();
    }
}

void QnActiSystemInfoChecker::handleFail()
{
    QnMutexLocker lock(&m_mutex);
    if (isLastCheckInCycleUnsafe())
    {
        m_cycleIsInProgress = false;
        m_lastSuccessfulAuth = boost::none;
        m_systemInfo = boost::none;
        m_failed = true;
        m_cacheExpirationTimer.restart();
    }
    else
    {
        tryToGetSystemInfoWithGivenAuthUnsafe(getNextAuthToCheckUnsafe());
    }
}

#endif // ENABLE_ACTI
