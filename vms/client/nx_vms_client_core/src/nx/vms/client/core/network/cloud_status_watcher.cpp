// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_status_watcher.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <network/cloud_system_data.h>
#include <nx/branding.h>
#include <nx/cloud/db/client/oauth_manager.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/cloud_system_data.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_token_remover.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <utils/common/delayed.h>
#include <utils/common/id.h>
#include <utils/common/synctime.h>

using namespace nx::cloud::db::api;
using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

static constexpr auto kSystemUpdateInterval = 10s;
static constexpr auto kReconnectInterval = 5s;
static constexpr int kUpdateSystemsTriesCount = 6;

QnCloudSystemList getCloudSystemList(const SystemDataExList& systemsList)
{
    using namespace nx::vms::common;

    QnCloudSystemList result;

    for (const SystemDataEx &systemData : systemsList.systems)
    {
        if (systemData.status != SystemStatus::activated)
            continue;

        const bool compatibleCustomization =
            ServerCompatibilityValidator::isCompatibleCustomization(QString::fromStdString(
                systemData.customization));

        if (!compatibleCustomization)
            continue;

        QnCloudSystem system;
        system.cloudId = QString::fromStdString(systemData.id);

        nx::vms::api::CloudSystemData data;
        system.localId = nx::reflect::json::deserialize(systemData.opaque, &data)
            ? data.localSystemId
            : guidFromArbitraryData(system.cloudId); //< Safety check, local id should be present.

        system.online = (systemData.stateOfHealth == SystemHealth::online);
        system.system2faEnabled = systemData.system2faEnabled;
        system.name = QString::fromStdString(systemData.name);
        system.ownerAccountEmail = QString::fromStdString(systemData.ownerAccountEmail());
        system.ownerFullName = QString::fromStdString(systemData.ownerFullName());
        system.authKey = {}; //< Intentionally skip saving.
        system.weight = systemData.usageFrequency;
        system.lastLoginTimeUtcMs = std::chrono::duration_cast<std::chrono::milliseconds>
            (systemData.lastLoginTime.time_since_epoch()).count();
        system.version = QString::fromStdString(systemData.version);
        system.organizationId = QString::fromStdString(systemData.organizationId());
        result.append(system);
    }

    return result;
}

} // namespace

struct CloudStatusWatcher::Private: public QObject
{
    CloudStatusWatcher* const q;
    std::unique_ptr<CloudConnectionFactory> cloudConnectionFactory;
    std::shared_ptr<Connection> cloudConnection;
    std::unique_ptr<Connection> resendActivationConnection;

    bool hasUpdateSystemsRequest = false;
    CloudStatusWatcher::Status status = CloudStatusWatcher::LoggedOut;
    CloudStatusWatcher::ErrorCode errorCode = CloudStatusWatcher::NoError;

    QnCloudSystemList cloudSystems;
    QnCloudSystemList recentCloudSystems;

    Private(CloudStatusWatcher* parent);

    void setCloudEnabled(bool enabled);
    bool cloudIsEnabled() const;
    bool is2FaEnabledForUser() const;

    using TimePoint = CloudStatusWatcher::TimePoint;
    /**
     * @brief Stops watcher from interacting with cloud from background
     * Background interaction with the cloud breaks user lockout feature in
     * ConnectToCloudDialog. Any request from the watcher is done with proper
     * user credentials, so it resets the counter for failed password attempts.
     * Mirrored from the parent class.
     * @param timeout: interaction will be automatically resumed after it expires.
     */
    void suppressCloudInteraction(CloudStatusWatcher::TimePoint::duration timeout);

    /**
     * @brief Resumes background interaction with the cloud.
     * Mirrored from the parent class.
     */
    void resumeCloudInteraction();

    bool checkSuppressed();

    const CloudAuthData& authData() const;
    bool setAuthData(const CloudAuthData& authData, bool initial, bool forced = false);

    void saveCredentials() const;

    void updateSystems();

    /** @return Whether request was actually sent. */
    bool updateSystemsInternal(int triesCount);
    void setCloudSystems(const QnCloudSystemList &newCloudSystems);
    void setRecentCloudSystems(const QnCloudSystemList &newRecentSystems);
    void updateCurrentCloudUserSecuritySettings();
    void updateStatusFromResultCode(ResultCode result);
    void setStatus(CloudStatusWatcher::Status newStatus, CloudStatusWatcher::ErrorCode error);

    void resetCloudConnection();
    void updateConnection(bool initial, bool forced = false);
    void issueAccessToken();
    void onAccessTokenIssued(ResultCode result, IssueTokenResponse response);
    void validateAccessToken();
    void scheduleConnectionRetryIfNeeded();

private:
    enum class SuppressionMode
    {
        Resumed,
        Suppressed,
        SuppressedUntil,
    };

private:
    bool m_cloudIsEnabled = true;

    SuppressionMode m_suppression = SuppressionMode::Resumed;
    // Time limit for cloud suppression.
    TimePoint m_suppressUntil;

    CloudAuthData m_authData;
    AccountSecuritySettings m_accountSecuritySettings;
    std::unique_ptr<CloudSessionTokenUpdater> m_tokenUpdater;
    CloudTokenRemover m_tokenRemover;
};

CloudStatusWatcher::CloudStatusWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    const auto correctOfflineState =
        [this]()
        {
            switch (d->status)
            {
                case CloudStatusWatcher::Online:
                    d->setRecentCloudSystems(cloudSystems());
                    break;
                case CloudStatusWatcher::LoggedOut:
                    d->setRecentCloudSystems(QnCloudSystemList());
                    d->resetCloudConnection();
                    break;
                default:
                    break;
            }
        };

    connect(this, &CloudStatusWatcher::statusChanged, this, correctOfflineState);

    connect(this, &CloudStatusWatcher::cloudSystemsChanged,
        d.get(), &CloudStatusWatcher::Private::setRecentCloudSystems);
    connect(this, &CloudStatusWatcher::recentCloudSystemsChanged, this,
        [this]()
        {
            appContext()->coreSettings()->recentCloudSystems = recentCloudSystems();
        });
}

CloudStatusWatcher::~CloudStatusWatcher()
{
}

nx::network::http::Credentials CloudStatusWatcher::credentials() const
{
    return d->authData().credentials;
}

nx::network::http::Credentials CloudStatusWatcher::remoteConnectionCredentials() const
{
    auto result = credentials();
    result.authToken = nx::network::http::BearerAuthToken(d->authData().refreshToken);
    return result;
}

QString CloudStatusWatcher::cloudLogin() const
{
    return QString::fromStdString(credentials().username);
}

bool CloudStatusWatcher::is2FaEnabledForUser() const
{
    return d->is2FaEnabledForUser();
}

void CloudStatusWatcher::logSession(const QString& cloudSystemId)
{
    if (!d->cloudConnection)
        return;

    d->cloudConnection->systemManager()->recordUserSessionStart(cloudSystemId.toStdString(),
        [this, cloudSystemId](ResultCode result)
        {
            if (result == ResultCode::ok)
                return;

            NX_INFO(this, "Error logging session %1: %2", cloudSystemId, result);
        });
}

void CloudStatusWatcher::resetAuthData()
{
    setAuthData(CloudAuthData());
    appContext()->coreSettings()->setCloudAuthData(CloudAuthData());
}

bool CloudStatusWatcher::setAuthData(const CloudAuthData& authData, bool forced)
{
    return d->setAuthData(authData, /*initial*/ false, forced);
}

bool CloudStatusWatcher::setInitialAuthData(const CloudAuthData& authData)
{
    return d->setAuthData(authData, /*initial*/ true);
}

CloudStatusWatcher::ErrorCode CloudStatusWatcher::error() const
{
    return d->errorCode;
}

CloudStatusWatcher::Status CloudStatusWatcher::status() const
{
    return d->status;
}

bool CloudStatusWatcher::isCloudEnabled() const
{
    return d->cloudIsEnabled();
}

void CloudStatusWatcher::suppressCloudInteraction(TimePoint::duration timeout)
{
    d->suppressCloudInteraction(timeout);
}

void CloudStatusWatcher::resumeCloudInteraction()
{
    d->resumeCloudInteraction();
}

void CloudStatusWatcher::updateSystems()
{
    d->updateSystems();
}

void CloudStatusWatcher::resendActivationEmail(const QString& email)
{
    if (!d->cloudConnectionFactory)
        d->cloudConnectionFactory = std::make_unique<CloudConnectionFactory>();

    if (!d->resendActivationConnection)
        d->resendActivationConnection = d->cloudConnectionFactory->createConnection();

    const auto callback =
        [this](ResultCode result, AccountConfirmationCode /*code*/)
        {
            const bool success =
                result == ResultCode::ok || result == ResultCode::partialContent;

            emit activationEmailResent(success);
        };

    const AccountEmail account{email.toStdString()};
    const auto accountManager = d->resendActivationConnection->accountManager();
    accountManager->reactivateAccount(account, nx::utils::guarded(this, callback));
}

std::optional<QnCloudSystem> CloudStatusWatcher::cloudSystem(const QString& systemId) const
{
    for (const auto& cloudSystem: d->cloudSystems)
    {
        if (cloudSystem.cloudId == systemId)
            return cloudSystem;
    }

    return {};
}

QnCloudSystemList CloudStatusWatcher::cloudSystems() const
{
    return d->cloudSystems;
}

QnCloudSystemList CloudStatusWatcher::recentCloudSystems() const
{
    return d->recentCloudSystems;
}

CloudStatusWatcher::Private::Private(CloudStatusWatcher *parent):
    QObject(parent),
    q(parent),
    recentCloudSystems(appContext()->coreSettings()->recentCloudSystems())
{
    QTimer* systemUpdateTimer = new QTimer(this);
    systemUpdateTimer->setInterval(kSystemUpdateInterval);
    systemUpdateTimer->callOnTimeout(
        [this]()
        {
             if (checkSuppressed())
                 q->updateSystems();

             updateCurrentCloudUserSecuritySettings();
        });
    systemUpdateTimer->start();

    m_tokenUpdater = std::make_unique<CloudSessionTokenUpdater>(this);

    connect(
        m_tokenUpdater.get(),
        &CloudSessionTokenUpdater::sessionTokenExpiring,
        this,
        &CloudStatusWatcher::Private::issueAccessToken);
}

bool CloudStatusWatcher::Private::cloudIsEnabled() const
{
    return m_cloudIsEnabled;
}

bool CloudStatusWatcher::Private::is2FaEnabledForUser() const
{
    // totpExistsForAccount field shows whether 2fa is enabled for cloud account.
    return m_accountSecuritySettings.totpExistsForAccount.value_or(false);
}

void CloudStatusWatcher::Private::setCloudEnabled(bool enabled)
{
    if (m_cloudIsEnabled == enabled)
        return;

    m_cloudIsEnabled = enabled;
    emit q->isCloudEnabledChanged();
}

void CloudStatusWatcher::Private::updateStatusFromResultCode(ResultCode result)
{
    setCloudEnabled(result != ResultCode::networkError
        && result != ResultCode::serviceUnavailable
        && result != ResultCode::accountBlocked);

    switch (result)
    {
        case ResultCode::ok:
            setStatus(CloudStatusWatcher::Online, CloudStatusWatcher::NoError);
            break;
        case ResultCode::badUsername:
            setStatus(CloudStatusWatcher::LoggedOut, CloudStatusWatcher::InvalidEmail);
            break;
        case ResultCode::notAuthorized:
        case ResultCode::forbidden:
            setStatus(CloudStatusWatcher::LoggedOut, CloudStatusWatcher::InvalidPassword);
            break;
        case ResultCode::accountNotActivated:
            setStatus(CloudStatusWatcher::LoggedOut, CloudStatusWatcher::AccountNotActivated);
            break;
        case ResultCode::accountBlocked:
            setStatus(
                CloudStatusWatcher::LoggedOut,
                CloudStatusWatcher::UserTemporaryLockedOut);
            break;
        default:
            setStatus(CloudStatusWatcher::Offline, CloudStatusWatcher::UnknownError);
            break;
    }
}

bool CloudStatusWatcher::Private::updateSystemsInternal(int triesCount)
{
    if (!cloudConnection || m_authData.credentials.authToken.empty())
        return false;

    if (status == CloudStatusWatcher::LoggedOut)
        return false;

    if (!checkSuppressed())
        return false;

    if (triesCount <= 0)
        return false;

    auto handler = nx::utils::AsyncHandlerExecutor(this).bind(
        [this, triesCount](ResultCode result, const SystemDataExList& systemsList)
        {
            if (!cloudConnection)
                return;

            QnCloudSystemList cloudSystems;
            if (result == ResultCode::badUsername && triesCount > 0)
            {
                // In some situations after mobile client awakeness from the suspension we have
                // expired cloud access token. It may take some time to update it, so we give a
                // chance and do the several tries.
                NX_DEBUG(this, "Access token is expired, waiting for the token refresh,\
                    Try #%1", kUpdateSystemsTriesCount - triesCount + 1);
                m_tokenUpdater->updateTokenIfNeeded();
                const auto request =
                    [this, tries = triesCount - 1]()
                    {
                        updateSystemsInternal(tries);
                    };
                executeDelayedParented(request, /*delay*/ 10s, this);
                return;
            }
            else if (result == ResultCode::ok)
            {
                NX_DEBUG(this, "Successfully updated systems list");
                cloudSystems = getCloudSystemList(systemsList);
                setCloudSystems(cloudSystems);
            }
            else
            {
                NX_ERROR(this, "Can't update systems list, error '%1'", result);
            }

            hasUpdateSystemsRequest = false;
            updateStatusFromResultCode(result);
        });

    NX_DEBUG(this, "Updating systems list");
    cloudConnection->systemManager()->getSystemsFiltered(Filter(), std::move(handler));
    return true;
}

void CloudStatusWatcher::Private::updateSystems()
{
    NX_DEBUG(this, "Try update systems list: has request is %1 ", hasUpdateSystemsRequest);

    if (hasUpdateSystemsRequest)
        return;

    hasUpdateSystemsRequest = updateSystemsInternal(kUpdateSystemsTriesCount);
}

void CloudStatusWatcher::Private::resetCloudConnection()
{
    cloudConnection.reset();
    hasUpdateSystemsRequest = false;
}

void CloudStatusWatcher::Private::updateConnection(bool initial, bool forced)
{
    const auto status =
        (initial || forced) ? CloudStatusWatcher::Offline : CloudStatusWatcher::LoggedOut;
    setStatus(status, CloudStatusWatcher::NoError);
    resetCloudConnection();

    const auto& credentials = m_authData.credentials;
    if (m_authData.empty())
    {
        const auto error = (initial || credentials.username.empty())
            ? CloudStatusWatcher::NoError
            : CloudStatusWatcher::InvalidPassword;
        setStatus(CloudStatusWatcher::LoggedOut, error);
        setCloudSystems(QnCloudSystemList());
        return;
    }

    if (!cloudConnectionFactory)
        cloudConnectionFactory = std::make_unique<CloudConnectionFactory>();

    cloudConnection = cloudConnectionFactory->createConnection();

    if (credentials.authToken.empty() || forced)
    {
        issueAccessToken();
    }
    else
    {
        cloudConnection->setCredentials(credentials);
        validateAccessToken();
    }
}

void CloudStatusWatcher::Private::onAccessTokenIssued(
    ResultCode result,
    IssueTokenResponse response)
{
    NX_DEBUG(
        this,
        "Issue token result: %1, error: %2",
        result,
        response.error.value_or(std::string()));

    if (!cloudConnection)
        return;

    if (result != ResultCode::ok)
    {
        updateStatusFromResultCode(result);
        scheduleConnectionRetryIfNeeded();
        return;
    }

    if (response.error)
    {
        if (*response.error == OauthManager::k2faRequiredError)
        {
            updateStatusFromResultCode(ResultCode::notAuthorized);
            return;
        }
        else
        {
            updateStatusFromResultCode(ResultCode::unknownError);
            return;
        }
    }

    m_authData.credentials.authToken =
        nx::network::http::BearerAuthToken(response.access_token);
    m_authData.refreshToken = response.refresh_token;
    m_authData.expiresAt = response.expires_at;
    m_authData.authorizationCode.clear();

    cloudConnection->setCredentials(m_authData.credentials);
    m_tokenUpdater->onTokenUpdated(m_authData.expiresAt);

    emit q->credentialsChanged();

    validateAccessToken();
};

void CloudStatusWatcher::Private::issueAccessToken()
{
    auto callback =
        [this](ResultCode result, IssueTokenResponse response)
        {
            onAccessTokenIssued(result, std::move(response));
        };

    if (!NX_ASSERT(cloudConnection))
        return;

    IssueTokenRequest request;
    request.client_id = CloudConnectionFactory::clientId();
    if (!m_authData.refreshToken.empty())
    {
        request.grant_type = GrantType::refresh_token;
        request.refresh_token = m_authData.refreshToken;
    }
    else if (!m_authData.authorizationCode.empty())
    {
        request.grant_type = GrantType::authorization_code;
        request.code = m_authData.authorizationCode;
    }
    else
    {
        NX_ASSERT(false, "No authorization data.");
    }

    m_tokenUpdater->issueToken(request, std::move(callback), this);
}

void CloudStatusWatcher::Private::validateAccessToken()
{
    auto callback = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](ResultCode result, ValidateTokenResponse response)
        {
            NX_DEBUG(this, "Validate token result: %1", result);
            if (!cloudConnection)
                return;

            if (result != ResultCode::ok)
            {
                updateStatusFromResultCode(result);
                return;
            }

            const bool loginChanged = (m_authData.credentials.username != response.username);
            m_authData.credentials.username = std::move(response.username);

            NX_DEBUG(this, "Access token valid, username: %1", q->cloudLogin());

            updateStatusFromResultCode(result);
            saveCredentials();

            if (result == ResultCode::ok)
                q->updateSystems();
            if (loginChanged)
                emit q->cloudLoginChanged();
        });

    cloudConnection->oauthManager()->legacyValidateToken(
        m_authData.credentials.authToken.value, std::move(callback));
}

void CloudStatusWatcher::Private::scheduleConnectionRetryIfNeeded()
{
    if (!cloudConnection || status != CloudStatusWatcher::Offline)
        return;

    QTimer::singleShot(
        kReconnectInterval,
        this,
        [this]()
        {
            if (!cloudConnection || status != CloudStatusWatcher::Offline)
                return;

            NX_DEBUG(this, "Retrying connection");
            issueAccessToken();
        });
}

void CloudStatusWatcher::Private::setStatus(
    CloudStatusWatcher::Status newStatus, CloudStatusWatcher::ErrorCode newErrorCode)
{
    const bool isNewStatus = (status != newStatus);
    const bool isNewErrorCode = (errorCode != newErrorCode);
    const bool isJustLoggedOut = status == Online && newStatus == LoggedOut;

    if (!isNewStatus && !isNewErrorCode)
        return;

    // We have to update both values atomically
    status = newStatus;
    errorCode = newErrorCode;
    NX_DEBUG(this, "Updating cloud status: %1, error: %2", status, errorCode);

    // This should catch the case when user changes cloud password from browser.
    if (isNewStatus
        && status == CloudStatusWatcher::LoggedOut
        && (errorCode == CloudStatusWatcher::InvalidPassword
            || errorCode == CloudStatusWatcher::InvalidEmail))
    {
        NX_WARNING(this, "Forcing logout. Session expired or password was changed.");
        m_accountSecuritySettings = {};
        q->resetAuthData();
        emit q->forcedLogout();
    }

    if (isNewStatus)
        emit q->statusChanged(status);

    if (isNewErrorCode && (errorCode != CloudStatusWatcher::NoError))
        emit q->errorChanged(errorCode);

    if (isJustLoggedOut && errorCode != NoError)
        emit q->loggedOutWithError();
}

void CloudStatusWatcher::Private::setCloudSystems(const QnCloudSystemList &newCloudSystems)
{
    if (cloudSystems == newCloudSystems)
        return;

    cloudSystems = newCloudSystems;

    emit q->cloudSystemsChanged(cloudSystems);
}

void CloudStatusWatcher::Private::setRecentCloudSystems(const QnCloudSystemList &newRecentCloudSystems)
{
    if (recentCloudSystems == newRecentCloudSystems)
        return;

    recentCloudSystems = newRecentCloudSystems;

    emit q->recentCloudSystemsChanged();
}

void CloudStatusWatcher::Private::updateCurrentCloudUserSecuritySettings()
{
    if (!cloudConnection)
        return;

    auto callback = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](ResultCode result, AccountSecuritySettings settings)
        {
            NX_VERBOSE(
                this, "Updated current security settings of the cloud user, status: %1.", result);

            bool has2FaChanged =
                m_accountSecuritySettings.totpExistsForAccount != settings.totpExistsForAccount;

            m_accountSecuritySettings = settings;

            if (has2FaChanged)
            {
                emit q->is2FaEnabledForUserChanged();
            }
        });

    cloudConnection->accountManager()->getSecuritySettings(std::move(callback));
}

void CloudStatusWatcher::Private::suppressCloudInteraction(
        TimePoint::duration timeout)
{
    if (timeout > std::chrono::milliseconds(0))
    {
        m_suppression = SuppressionMode::SuppressedUntil;
        // Updating suppression time only if it will be later than the current limit.
        TimePoint newTime = TimePoint::clock::now() + timeout;
        if (newTime > m_suppressUntil)
            m_suppressUntil = newTime;
    }
    else
    {
        m_suppression = SuppressionMode::Suppressed;
        m_suppressUntil = TimePoint::clock::now();
    }
}

void CloudStatusWatcher::Private::resumeCloudInteraction()
{
    m_suppression = SuppressionMode::Resumed;
}

bool CloudStatusWatcher::Private::checkSuppressed()
{
    if (m_suppression == SuppressionMode::Suppressed)
        return false;
    if (m_suppression == SuppressionMode::SuppressedUntil)
    {
        if (m_suppressUntil < TimePoint::clock::now())
        {
            m_suppression = SuppressionMode::Resumed;
            return true;
        }
        return false;
    }
    return true;
}

bool CloudStatusWatcher::Private::setAuthData(const CloudAuthData& authData, bool initial, bool forced)
{
    NX_ASSERT(!authData.credentials.authToken.isPassword());

    const bool userChanged = (m_authData.credentials.username != authData.credentials.username);

    // Removing current tokens from cloud.
    m_tokenRemover.removeToken(m_authData.refreshToken, cloudConnection);
    m_tokenRemover.removeToken(m_authData.credentials.authToken.value, cloudConnection);

    m_authData = authData;
    m_tokenUpdater->onTokenUpdated(m_authData.expiresAt);

    updateConnection(initial, forced);

    if (userChanged)
        emit q->cloudLoginChanged();

    return true;
}

void CloudStatusWatcher::Private::saveCredentials() const
{
    if (status == CloudStatusWatcher::LoggedOut)
        return;

    appContext()->coreSettings()->setCloudAuthData(m_authData);
}

const CloudAuthData& CloudStatusWatcher::Private::authData() const
{
    return m_authData;
}

} // namespace nx::vms::client::core
