// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_status_watcher.h"

#include <algorithm>

#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <client_core/client_core_settings.h>
#include <helpers/system_helpers.h>
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
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_token_remover.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <utils/common/delayed.h>
#include <utils/common/id.h>
#include <utils/common/synctime.h>

using namespace nx::cloud::db;
using namespace nx::vms::client::core;
using namespace std::chrono;

namespace {

static constexpr auto kSystemUpdateInterval = 10s;
static constexpr auto kReconnectInterval = 5s;
static constexpr int kUpdateSystemsTriesCount = 6;

QnCloudSystemList getCloudSystemList(const api::SystemDataExList& systemsList)
{
    using namespace nx::vms::common;

    QnCloudSystemList result;

    for (const nx::cloud::db::api::SystemDataEx &systemData : systemsList.systems)
    {
        if (systemData.status != nx::cloud::db::api::SystemStatus::activated)
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

        system.online = (systemData.stateOfHealth == nx::cloud::db::api::SystemHealth::online);
        system.system2faEnabled = systemData.system2faEnabled;
        system.name = QString::fromStdString(systemData.name);
        system.ownerAccountEmail = QString::fromStdString(systemData.ownerAccountEmail);
        system.ownerFullName = QString::fromStdString(systemData.ownerFullName);
        system.authKey = {}; //< Intentionally skip saving.
        system.weight = systemData.usageFrequency;
        system.lastLoginTimeUtcMs = std::chrono::duration_cast<std::chrono::milliseconds>
            (systemData.lastLoginTime.time_since_epoch()).count();
        system.version = QString::fromStdString(systemData.version);
        result.append(system);
    }

    return result;
}

}

class QnCloudStatusWatcherPrivate : public QObject
{
    QnCloudStatusWatcher* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusWatcher)

public:
    std::unique_ptr<CloudConnectionFactory> cloudConnectionFactory;
    std::shared_ptr<api::Connection> cloudConnection;
    std::unique_ptr<api::Connection> resendActivationConnection;

    bool hasUpdateSystemsRequest = false;
    QnCloudStatusWatcher::Status status = QnCloudStatusWatcher::LoggedOut;
    QnCloudStatusWatcher::ErrorCode errorCode = QnCloudStatusWatcher::NoError;

    QnCloudSystemList cloudSystems;
    QnCloudSystemList recentCloudSystems;

public:
    QnCloudStatusWatcherPrivate(QnCloudStatusWatcher* parent);

    void setCloudEnabled(bool enabled);
    bool cloudIsEnabled() const;
    bool is2FaEnabledForUser() const;

    using TimePoint = QnCloudStatusWatcher::TimePoint;
    /**
     * @brief Stops watcher from interacting with cloud from background
     * Background interaction with the cloud breaks user lockout feature in
     * ConnectToCloudDialog. Any request from the watcher is done with proper
     * user credentials, so it resets the counter for failed password attempts.
     * Mirrored from the parent class.
     * @param timeout: interaction will be automatically resumed after it expires.
     */
    void suppressCloudInteraction(QnCloudStatusWatcher::TimePoint::duration timeout);

    /**
     * @brief Resumes background interaction with the cloud.
     * Mirrored from the parent class.
     */
    void resumeCloudInteraction();

    bool checkSuppressed();

    const CloudAuthData& authData() const;
    bool setAuthData(const CloudAuthData& authData, bool initial);

    void saveCredentials() const;

private:
    void updateSystems();

    /** @return Whether request was actually sent. */
    bool updateSystemsInternal(int triesCount);
    void setCloudSystems(const QnCloudSystemList &newCloudSystems);
    void setRecentCloudSystems(const QnCloudSystemList &newRecentSystems);
    void updateCurrentCloudUserSecuritySettings();
    void updateStatusFromResultCode(api::ResultCode result);
    void setStatus(QnCloudStatusWatcher::Status newStatus, QnCloudStatusWatcher::ErrorCode error);

    void resetCloudConnection();
    void updateConnection(bool initial);
    void issueAccessToken();
    void onAccessTokenIssued(api::ResultCode result, api::IssueTokenResponse response);
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
    api::AccountSecuritySettings m_accountSecuritySettings;
    std::unique_ptr<CloudSessionTokenUpdater> m_tokenUpdater;
    CloudTokenRemover m_tokenRemover;
};

template<> QnCloudStatusWatcher* Singleton<QnCloudStatusWatcher>::s_instance = nullptr;

QnCloudStatusWatcher::QnCloudStatusWatcher(QObject* parent):
    base_type(parent),
    d_ptr(new QnCloudStatusWatcherPrivate(this))
{
    const auto correctOfflineState = [this]()
        {
            Q_D(QnCloudStatusWatcher);
            switch (d->status)
            {
                case QnCloudStatusWatcher::Online:
                    d->setRecentCloudSystems(cloudSystems());
                    break;
                case QnCloudStatusWatcher::LoggedOut:
                    d->setRecentCloudSystems(QnCloudSystemList());
                    d->resetCloudConnection();
                    break;
                default:
                    break;
            }
        };

    connect(this, &QnCloudStatusWatcher::statusChanged, this, correctOfflineState);

    Q_D(QnCloudStatusWatcher);

    connect(this, &QnCloudStatusWatcher::cloudSystemsChanged,
        d, &QnCloudStatusWatcherPrivate::setRecentCloudSystems);
    connect(this, &QnCloudStatusWatcher::recentCloudSystemsChanged, this,
        [this]()
        {
            qnClientCoreSettings->setRecentCloudSystems(recentCloudSystems());
            qnClientCoreSettings->save();
        });
}

QnCloudStatusWatcher::~QnCloudStatusWatcher()
{
    Q_D(const QnCloudStatusWatcher);
    d->saveCredentials();
}

nx::network::http::Credentials QnCloudStatusWatcher::credentials() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->authData().credentials;
}

nx::network::http::Credentials QnCloudStatusWatcher::remoteConnectionCredentials() const
{
    Q_D(const QnCloudStatusWatcher);
    auto result = credentials();
    result.authToken = nx::network::http::BearerAuthToken(d->authData().refreshToken);
    return result;
}

QString QnCloudStatusWatcher::cloudLogin() const
{
    return QString::fromStdString(credentials().username);
}

bool QnCloudStatusWatcher::is2FaEnabledForUser() const
{
    Q_D(const QnCloudStatusWatcher);

    return d->is2FaEnabledForUser();
}

void QnCloudStatusWatcher::logSession(const QString& cloudSystemId)
{
    Q_D(QnCloudStatusWatcher);

    if (!d->cloudConnection)
        return;

    d->cloudConnection->systemManager()->recordUserSessionStart(cloudSystemId.toStdString(),
        [cloudSystemId](api::ResultCode result)
        {
            if (result == api::ResultCode::ok)
                return;

            NX_INFO(typeid(QnCloudStatusWatcher), lit("Error logging session %1: %2")
                .arg(cloudSystemId)
                .arg(QString::fromStdString(api::toString(result))));
        });
}

void QnCloudStatusWatcher::resetAuthData()
{
    setAuthData(CloudAuthData());
    nx::vms::client::core::helpers::saveCloudCredentials(CloudAuthData());
}

bool QnCloudStatusWatcher::setAuthData(const CloudAuthData& authData)
{
    Q_D(QnCloudStatusWatcher);
    return d->setAuthData(authData, /*initial*/ false);
}

bool QnCloudStatusWatcher::setInitialAuthData(const CloudAuthData& authData)
{
    Q_D(QnCloudStatusWatcher);
    return d->setAuthData(authData, /*initial*/ true);
}

QnCloudStatusWatcher::ErrorCode QnCloudStatusWatcher::error() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->errorCode;
}

QnCloudStatusWatcher::Status QnCloudStatusWatcher::status() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->status;
}

bool QnCloudStatusWatcher::isCloudEnabled() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->cloudIsEnabled();
}

void QnCloudStatusWatcher::suppressCloudInteraction(TimePoint::duration timeout)
{
    Q_D(QnCloudStatusWatcher);
    d->suppressCloudInteraction(timeout);
}

void QnCloudStatusWatcher::resumeCloudInteraction()
{
    Q_D(QnCloudStatusWatcher);
    d->resumeCloudInteraction();
}

void QnCloudStatusWatcher::updateSystems()
{
    Q_D(QnCloudStatusWatcher);
    d->updateSystems();
}

void QnCloudStatusWatcher::resendActivationEmail(const QString& email)
{
    Q_D(QnCloudStatusWatcher);
    if (!d->cloudConnectionFactory)
        d->cloudConnectionFactory = std::make_unique<CloudConnectionFactory>();

    if (!d->resendActivationConnection)
        d->resendActivationConnection = d->cloudConnectionFactory->createConnection();

    const auto callback =
        [this](api::ResultCode result, api::AccountConfirmationCode /*code*/)
        {
            const bool success =
                result == api::ResultCode::ok || result == api::ResultCode::partialContent;

            emit activationEmailResent(success);
        };

    const api::AccountEmail account{email.toStdString()};
    const auto accountManager = d->resendActivationConnection->accountManager();
    accountManager->reactivateAccount(account, nx::utils::guarded(this, callback));
}

std::optional<QnCloudSystem> QnCloudStatusWatcher::cloudSystem(const QString& systemId) const
{
    Q_D(const QnCloudStatusWatcher);

    for (const auto& cloudSystem: d->cloudSystems)
    {
        if (cloudSystem.cloudId == systemId)
            return cloudSystem;
    }

    return {};
}

QnCloudSystemList QnCloudStatusWatcher::cloudSystems() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->cloudSystems;
}

QnCloudSystemList QnCloudStatusWatcher::recentCloudSystems() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->recentCloudSystems;
}

QnCloudStatusWatcherPrivate::QnCloudStatusWatcherPrivate(QnCloudStatusWatcher *parent):
    QObject(parent),
    q_ptr(parent),
    recentCloudSystems(qnClientCoreSettings->recentCloudSystems())
{
    Q_Q(QnCloudStatusWatcher);

    QTimer* systemUpdateTimer = new QTimer(this);
    systemUpdateTimer->setInterval(kSystemUpdateInterval);
    systemUpdateTimer->callOnTimeout(
        [this, q]()
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
        &QnCloudStatusWatcherPrivate::issueAccessToken);
}

bool QnCloudStatusWatcherPrivate::cloudIsEnabled() const
{
    return m_cloudIsEnabled;
}

bool QnCloudStatusWatcherPrivate::is2FaEnabledForUser() const
{
    // totpExistsForAccount field shows whether 2fa is enabled for cloud account.
    return m_accountSecuritySettings.totpExistsForAccount.value_or(false);
}

void QnCloudStatusWatcherPrivate::setCloudEnabled(bool enabled)
{
    Q_Q(QnCloudStatusWatcher);

    if (m_cloudIsEnabled == enabled)
        return;

    m_cloudIsEnabled = enabled;
    emit q->isCloudEnabledChanged();
}

void QnCloudStatusWatcherPrivate::updateStatusFromResultCode(api::ResultCode result)
{
    setCloudEnabled(result != api::ResultCode::networkError
        && result != api::ResultCode::serviceUnavailable
        && result != api::ResultCode::accountBlocked);

    switch (result)
    {
        case api::ResultCode::ok:
            setStatus(QnCloudStatusWatcher::Online, QnCloudStatusWatcher::NoError);
            break;
        case api::ResultCode::badUsername:
            setStatus(QnCloudStatusWatcher::LoggedOut, QnCloudStatusWatcher::InvalidEmail);
            break;
        case api::ResultCode::notAuthorized:
        case api::ResultCode::forbidden:
            setStatus(QnCloudStatusWatcher::LoggedOut, QnCloudStatusWatcher::InvalidPassword);
            break;
        case api::ResultCode::accountNotActivated:
            setStatus(QnCloudStatusWatcher::LoggedOut, QnCloudStatusWatcher::AccountNotActivated);
            break;
        case api::ResultCode::accountBlocked:
            setStatus(
                QnCloudStatusWatcher::LoggedOut,
                QnCloudStatusWatcher::UserTemporaryLockedOut);
            break;
        default:
            setStatus(QnCloudStatusWatcher::Offline, QnCloudStatusWatcher::UnknownError);
            break;
    }
}

bool QnCloudStatusWatcherPrivate::updateSystemsInternal(int triesCount)
{
    if (!cloudConnection || m_authData.credentials.authToken.empty())
        return false;

    if (status == QnCloudStatusWatcher::LoggedOut)
        return false;

    if (!checkSuppressed())
        return false;

    if (triesCount <= 0)
        return false;

    auto handler = nx::utils::AsyncHandlerExecutor(this).bind(
        [this, triesCount](api::ResultCode result, const api::SystemDataExList& systemsList)
        {
            if (!cloudConnection)
                return;

            QnCloudSystemList cloudSystems;
            if (result == api::ResultCode::badUsername && triesCount > 0)
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
            else if (result == api::ResultCode::ok)
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
    cloudConnection->systemManager()->getSystemsFiltered(api::Filter(), std::move(handler));
    return true;
}

void QnCloudStatusWatcherPrivate::updateSystems()
{
    NX_DEBUG(this, "Try update systems list: has request is %1 ", hasUpdateSystemsRequest);

    if (hasUpdateSystemsRequest)
        return;

    hasUpdateSystemsRequest = updateSystemsInternal(kUpdateSystemsTriesCount);
}

void QnCloudStatusWatcherPrivate::resetCloudConnection()
{
    cloudConnection.reset();
    hasUpdateSystemsRequest = false;
}

void QnCloudStatusWatcherPrivate::updateConnection(bool initial)
{
    const auto status = (initial
        ? QnCloudStatusWatcher::Offline
        : QnCloudStatusWatcher::LoggedOut);
    setStatus(status, QnCloudStatusWatcher::NoError);
    resetCloudConnection();

    const auto& credentials = m_authData.credentials;
    if (m_authData.empty())
    {
        const auto error = (initial || credentials.username.empty())
            ? QnCloudStatusWatcher::NoError
            : QnCloudStatusWatcher::InvalidPassword;
        setStatus(QnCloudStatusWatcher::LoggedOut, error);
        setCloudSystems(QnCloudSystemList());
        return;
    }

    if (!cloudConnectionFactory)
        cloudConnectionFactory = std::make_unique<CloudConnectionFactory>();

    cloudConnection = cloudConnectionFactory->createConnection();

    if (credentials.authToken.empty())
    {
        issueAccessToken();
    }
    else
    {
        cloudConnection->setCredentials(credentials);
        validateAccessToken();
    }
}

void QnCloudStatusWatcherPrivate::onAccessTokenIssued(
    api::ResultCode result, api::IssueTokenResponse response)
{
    NX_DEBUG(
        this,
        "Issue token result: %1, error: %2",
        result,
        response.error.value_or(std::string()));

    if (!cloudConnection)
        return;

    if (result != api::ResultCode::ok)
    {
        updateStatusFromResultCode(result);
        scheduleConnectionRetryIfNeeded();
        return;
    }

    if (response.error)
    {
        if (*response.error == api::OauthManager::k2faRequiredError)
        {
            updateStatusFromResultCode(api::ResultCode::notAuthorized);
            return;
        }
        else
        {
            updateStatusFromResultCode(api::ResultCode::unknownError);
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

    Q_Q(QnCloudStatusWatcher);
    emit q->credentialsChanged();

    validateAccessToken();
};

void QnCloudStatusWatcherPrivate::issueAccessToken()
{
    auto callback =
        [this](api::ResultCode result, api::IssueTokenResponse response)
        {
            onAccessTokenIssued(result, std::move(response));
        };

    if (!NX_ASSERT(cloudConnection))
        return;

    api::IssueTokenRequest request;
    request.client_id = CloudConnectionFactory::clientId();
    if (!m_authData.refreshToken.empty())
    {
        request.grant_type = api::GrantType::refreshToken;
        request.refresh_token = m_authData.refreshToken;
    }
    else if (!m_authData.authorizationCode.empty())
    {
        request.grant_type = api::GrantType::authorizationCode;
        request.code = m_authData.authorizationCode;
    }
    else
    {
        NX_ASSERT(false, "No authorization data.");
    }

    m_tokenUpdater->issueToken(request, std::move(callback), this);
}

void QnCloudStatusWatcherPrivate::validateAccessToken()
{
    auto callback = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](api::ResultCode result, api::ValidateTokenResponse response)
        {
            NX_DEBUG(this, "Validate token result: %1", result);
            if (!cloudConnection)
                return;

            if (result != api::ResultCode::ok)
            {
                updateStatusFromResultCode(result);
                return;
            }

            const bool loginChanged = (m_authData.credentials.username != response.username);
            m_authData.credentials.username = std::move(response.username);

            Q_Q(QnCloudStatusWatcher);
            NX_DEBUG(this, "Access token valid, username: %1", q->cloudLogin());

            updateStatusFromResultCode(result);
            saveCredentials();

            if (result == api::ResultCode::ok)
                q->updateSystems();
            if (loginChanged)
                emit q->cloudLoginChanged();
        });

    cloudConnection->oauthManager()->validateToken(
        m_authData.credentials.authToken.value, std::move(callback));
}

void QnCloudStatusWatcherPrivate::scheduleConnectionRetryIfNeeded()
{
    if (!cloudConnection || status != QnCloudStatusWatcher::Offline)
        return;

    QTimer::singleShot(
        kReconnectInterval,
        this,
        [this]()
        {
            if (!cloudConnection || status != QnCloudStatusWatcher::Offline)
                return;

            NX_DEBUG(this, "Retrying connection");
            issueAccessToken();
        });
}

void QnCloudStatusWatcherPrivate::setStatus(
    QnCloudStatusWatcher::Status newStatus, QnCloudStatusWatcher::ErrorCode newErrorCode)
{
    const bool isNewStatus = (status != newStatus);
    const bool isNewErrorCode = (errorCode != newErrorCode);

    if (!isNewStatus && !isNewErrorCode)
        return;

    // We have to update both values atomically
    status = newStatus;
    errorCode = newErrorCode;
    NX_DEBUG(this, "Updating cloud status: %1, error: %2", status, errorCode);

    Q_Q(QnCloudStatusWatcher);
    // This should catch the case when user changes cloud password from browser.
    if (isNewStatus
        && status == QnCloudStatusWatcher::LoggedOut
        && (errorCode == QnCloudStatusWatcher::InvalidPassword
            || errorCode == QnCloudStatusWatcher::InvalidEmail))
    {
        NX_WARNING(this, "Forcing logout. Session expired or password was changed.");
        m_accountSecuritySettings = {};
        q->resetAuthData();
        emit q->forcedLogout();
    }

    if (isNewStatus)
        emit q->statusChanged(status);

    if (isNewErrorCode && (errorCode != QnCloudStatusWatcher::NoError))
        emit q->errorChanged(errorCode);
}

void QnCloudStatusWatcherPrivate::setCloudSystems(const QnCloudSystemList &newCloudSystems)
{
    if (cloudSystems == newCloudSystems)
        return;

    Q_Q(QnCloudStatusWatcher);

    cloudSystems = newCloudSystems;

    emit q->cloudSystemsChanged(cloudSystems);
}

void QnCloudStatusWatcherPrivate::setRecentCloudSystems(const QnCloudSystemList &newRecentCloudSystems)
{
    if (recentCloudSystems == newRecentCloudSystems)
        return;

    recentCloudSystems = newRecentCloudSystems;

    Q_Q(QnCloudStatusWatcher);
    emit q->recentCloudSystemsChanged();
}

void QnCloudStatusWatcherPrivate::updateCurrentCloudUserSecuritySettings()
{
    if (!cloudConnection)
        return;

    auto callback = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](api::ResultCode result, api::AccountSecuritySettings settings)
        {
            NX_VERBOSE(
                this, "Updated current security settings of the cloud user, status: %1.", result);

            bool has2FaChanged =
                m_accountSecuritySettings.totpExistsForAccount != settings.totpExistsForAccount;

            m_accountSecuritySettings = settings;

            if (has2FaChanged)
            {
                Q_Q(QnCloudStatusWatcher);
                emit q->is2FaEnabledForUserChanged();
            }
        });

    cloudConnection->accountManager()->getSecuritySettings(std::move(callback));
}

void QnCloudStatusWatcherPrivate::suppressCloudInteraction(
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

void QnCloudStatusWatcherPrivate::resumeCloudInteraction()
{
    m_suppression = SuppressionMode::Resumed;
}

bool QnCloudStatusWatcherPrivate::checkSuppressed()
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

bool QnCloudStatusWatcherPrivate::setAuthData(const CloudAuthData& authData, bool initial)
{
    Q_Q(QnCloudStatusWatcher);
    NX_ASSERT(!authData.credentials.authToken.isPassword());

    const bool userChanged = (m_authData.credentials.username != authData.credentials.username);

    // Removing current tokens from cloud.
    m_tokenRemover.removeToken(m_authData.refreshToken, cloudConnection);
    m_tokenRemover.removeToken(m_authData.credentials.authToken.value, cloudConnection);

    m_authData = authData;
    m_tokenUpdater->onTokenUpdated(m_authData.expiresAt);

    updateConnection(initial);

    if (userChanged)
        emit q->cloudLoginChanged();

    return true;
}

void QnCloudStatusWatcherPrivate::saveCredentials() const
{
    if (status == QnCloudStatusWatcher::LoggedOut)
        return;

    nx::vms::client::core::helpers::saveCloudCredentials(m_authData);
}

const CloudAuthData& QnCloudStatusWatcherPrivate::authData() const
{
    return m_authData;
}
