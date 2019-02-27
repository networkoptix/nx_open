#include "cloud_status_watcher.h"

#include <algorithm>

#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <api/global_settings.h>
#include <api/runtime_info_manager.h>

#include <common/common_module.h>

#include <client_core/client_core_settings.h>
#include <client_core/connection_context_aware.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

#include <cloud/cloud_connection.h>
#include <network/cloud_system_data.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <utils/common/id.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/cloud_system_data.h>
#include <helpers/system_helpers.h>

using namespace nx::cloud::db;

//#define DEBUG_CLOUD_STATUS_WATCHER
#ifdef DEBUG_CLOUD_STATUS_WATCHER
#define TRACE(...) qDebug() << "QnCloudStatusWatcher: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif

namespace {

const auto kCloudSystemJsonHolderTag = lit("json");

const int kUpdateIntervalMs = 5 * 1000;

bool isCustomizationCompatible(const QString& customization, bool isMobile)
{
    const auto currentCustomization = QnAppInfo::customizationName();

    if (customization.isEmpty() || currentCustomization.isEmpty())
        return true;

    return currentCustomization == customization
        || (isMobile
            && currentCustomization.section(L'_', 0, 0) == customization.section(L'_', 0, 0));
}

QnCloudSystemList getCloudSystemList(const api::SystemDataExList& systemsList, bool isMobile)
{
    QnCloudSystemList result;

    for (const api::SystemDataEx &systemData : systemsList.systems)
    {
        if (systemData.status != api::SystemStatus::activated)
            continue;

        const auto customization = QString::fromStdString(systemData.customization);

        if (!isCustomizationCompatible(customization, isMobile))
            continue;

        auto data = QJson::deserialized<nx::vms::api::CloudSystemData>(
            QByteArray::fromStdString(systemData.opaque));

        QnCloudSystem system;
        system.cloudId = QString::fromStdString(systemData.id);

        system.localId = data.localSystemId;
        if (system.localId.isNull())
            system.localId = guidFromArbitraryData(system.cloudId);

        system.online = (systemData.stateOfHealth == nx::cloud::db::api::SystemHealth::online);
        system.name = QString::fromStdString(systemData.name);
        system.ownerAccountEmail = QString::fromStdString(systemData.ownerAccountEmail);
        system.ownerFullName = QString::fromStdString(systemData.ownerFullName);
        system.authKey = {}; //< Intentionally skip saving.
        system.weight = systemData.usageFrequency;
        system.lastLoginTimeUtcMs = std::chrono::duration_cast<std::chrono::milliseconds>
            (systemData.lastLoginTime.time_since_epoch()).count();
        result.append(system);
    }

    return result;
}

}

class QnCloudStatusWatcherPrivate : public QObject, public QnConnectionContextAware
{
    QnCloudStatusWatcher* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusWatcher)

public:
    QnCloudStatusWatcherPrivate(QnCloudStatusWatcher* parent);

    std::string cloudHost;
    int cloudPort;
    nx::vms::common::Credentials credentials;
    QString effectiveUserName;
    bool stayConnected;
    QnCloudStatusWatcher::ErrorCode errorCode;

    QTimer *updateTimer;

    std::unique_ptr<api::Connection> cloudConnection;
    std::unique_ptr<api::Connection> temporaryConnection;
    std::unique_ptr<api::Connection> resendActivationConnection;

    QnCloudStatusWatcher::Status status;

    QnCloudSystemList cloudSystems;
    QnCloudSystemList recentCloudSystems;
    QnCloudSystem currentSystem;
    api::TemporaryCredentials temporaryCredentials;
    bool isMobile = false;

public:
    void updateConnection(bool initial = false);
    void setCloudEnabled(bool enabled);
    bool cloudIsEnabled() const;

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

private:
    void setStatus(QnCloudStatusWatcher::Status newStatus,
        QnCloudStatusWatcher::ErrorCode error);
    void setCloudSystems(const QnCloudSystemList &newCloudSystems);
    void setRecentCloudSystems(const QnCloudSystemList &newRecentSystems);
    void updateCurrentSystem();
    void updateCurrentAccount();
    void createTemporaryCredentials();
    void prolongTemporaryCredentials();
    // Updates current status from the response code.
    void updateStatusFromResultCode(api::ResultCode result);

private:
    QTimer* m_pingTimer;
    bool m_cloudIsEnabled;

    enum class SuppressionMode
    {
        Resumed,
        Suppressed,
        SuppressedUntil,
    };

    SuppressionMode m_suppression = SuppressionMode::Resumed;

    // Time limit for cloud suppression.
    TimePoint m_suppressUntil;
};

QnCloudStatusWatcher::QnCloudStatusWatcher(QObject* parent, bool isMobile):
    base_type(parent),
    QnCommonModuleAware(parent),
    d_ptr(new QnCloudStatusWatcherPrivate(this))
{
    d_ptr->isMobile = isMobile;
    setStayConnected(!nx::vms::client::core::settings()->cloudCredentials().password.isEmpty());

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
                    d->cloudConnection.reset();
                    break;
                default:
                    break;
            }
        };

    connect(this, &QnCloudStatusWatcher::statusChanged, this, correctOfflineState);
    connect(this, &QnCloudStatusWatcher::stayConnectedChanged, this, correctOfflineState);

    Q_D(QnCloudStatusWatcher);
    connect(this, &QnCloudStatusWatcher::cloudSystemsChanged,
        d, &QnCloudStatusWatcherPrivate::updateCurrentSystem);
    connect(this, &QnCloudStatusWatcher::cloudSystemsChanged,
        d, &QnCloudStatusWatcherPrivate::setRecentCloudSystems);
    connect(this, &QnCloudStatusWatcher::recentCloudSystemsChanged, this,
        [this]()
        {
            qnClientCoreSettings->setRecentCloudSystems(recentCloudSystems());
            qnClientCoreSettings->save();
        });

    using namespace nx::vms::client::core;

    // TODO: #GDM store temporary credentials
    setCredentials(settings()->cloudCredentials(), true);

    connect(&settings()->cloudCredentials, &Settings::BaseProperty::changed, this,
        [this]()
        {
            setStayConnected(!settings()->cloudCredentials().password.isEmpty());
        });
}

QnCloudStatusWatcher::~QnCloudStatusWatcher()
{
}

nx::vms::common::Credentials QnCloudStatusWatcher::credentials() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->credentials;
}

QString QnCloudStatusWatcher::cloudLogin() const
{
    return credentials().user;
}

QString QnCloudStatusWatcher::cloudPassword() const
{
    return credentials().password;
}

QString QnCloudStatusWatcher::effectiveUserName() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->effectiveUserName;
}

void QnCloudStatusWatcher::setEffectiveUserName(const QString& value)
{
    Q_D(QnCloudStatusWatcher);
    if (d->effectiveUserName == value)
        return;

    d->effectiveUserName = value;
    emit effectiveUserNameChanged();
}

bool QnCloudStatusWatcher::stayConnected() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->stayConnected;
}

void QnCloudStatusWatcher::setStayConnected(bool value)
{
    Q_D(QnCloudStatusWatcher);
    if (d->stayConnected == value)
        return;

    d->stayConnected = value;
    emit stayConnectedChanged();
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

void QnCloudStatusWatcher::resetCredentials(bool keepUser)
{
    setCredentials({keepUser ? qnCloudStatusWatcher->effectiveUserName() : QString(), QString()});
}

bool QnCloudStatusWatcher::setCredentials(
    const nx::vms::common::Credentials& credentials,
    bool initial)
{
    Q_D(QnCloudStatusWatcher);

    const auto loweredCredentials =
        nx::vms::common::Credentials(credentials.user.toLower(), credentials.password);

    if (d->credentials == loweredCredentials)
        return false;

    const bool userChanged = (d->credentials.user != loweredCredentials.user);
    const bool passwordChanged = (d->credentials.password != loweredCredentials.password);

    d->credentials = loweredCredentials;

    d->updateConnection(initial);

    emit credentialsChanged();

    if (userChanged)
        emit this->loginChanged();
    if (passwordChanged)
        emit this->passwordChanged();

    return true;
}

nx::vms::common::Credentials QnCloudStatusWatcher::createTemporaryCredentials()
{
    Q_D(QnCloudStatusWatcher);
    const nx::vms::common::Credentials result(
        QString::fromStdString(d->temporaryCredentials.login),
        QString::fromStdString(d->temporaryCredentials.password));
    d->createTemporaryCredentials();
    return result;
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

void QnCloudStatusWatcherPrivate::updateStatusFromResultCode(api::ResultCode result)
{
    setCloudEnabled(result != api::ResultCode::networkError
        && result != api::ResultCode::serviceUnavailable
        && result != api::ResultCode::accountBlocked);

    switch (result)
    {
        case api::ResultCode::ok:
            setStatus(QnCloudStatusWatcher::Online,
                QnCloudStatusWatcher::NoError);
            break;
        case api::ResultCode::badUsername:
            setStatus(QnCloudStatusWatcher::LoggedOut,
                QnCloudStatusWatcher::InvalidEmail);
            break;
        case api::ResultCode::notAuthorized:
            setStatus(QnCloudStatusWatcher::LoggedOut,
                QnCloudStatusWatcher::InvalidPassword);
            break;
        case api::ResultCode::accountNotActivated:
            setStatus(QnCloudStatusWatcher::LoggedOut,
                QnCloudStatusWatcher::AccountNotActivated);
            break;
        case api::ResultCode::accountBlocked:
            setStatus(QnCloudStatusWatcher::LoggedOut,
                QnCloudStatusWatcher::UserTemporaryLockedOut);
            break;
        default:
            setStatus(QnCloudStatusWatcher::Offline,
                QnCloudStatusWatcher::UnknownError);
            break;
    }
}

void QnCloudStatusWatcher::updateSystems()
{
    Q_D(QnCloudStatusWatcher);

    if (!d->cloudConnection)
        return;

    const bool isMobile = d->isMobile;

    if (status() == LoggedOut)
        return;

    if (!d->checkSuppressed())
        return;

    QPointer<QnCloudStatusWatcher> guard(this);
    d->cloudConnection->systemManager()->getSystemsFiltered(api::Filter(),
        [this, guard, isMobile](api::ResultCode result, const api::SystemDataExList& systemsList)
        {
            if (!guard)
                return;

            const auto handler =
                [this, guard, result, systemsList, isMobile]()
                {
                    if (!guard)
                        return;

                    Q_D(QnCloudStatusWatcher);
                    if (!d->cloudConnection)
                        return;

                    QnCloudSystemList cloudSystems;
                    if (result == api::ResultCode::ok)
                    {
                        cloudSystems = getCloudSystemList(systemsList, isMobile);
                        d->setCloudSystems(cloudSystems);
                    }
                    d->updateStatusFromResultCode(result);
                };

            executeDelayed(handler, 0, guard->thread());
        }
    );
}

void QnCloudStatusWatcher::resendActivationEmail(const QString& email)
{
    Q_D(QnCloudStatusWatcher);
    if (!d->resendActivationConnection)
        d->resendActivationConnection = qnCloudConnectionProvider->createConnection();

    const auto callback =
        [this](api::ResultCode result, api::AccountConfirmationCode code)
        {
            const bool success =
                result == api::ResultCode::ok || result == api::ResultCode::partialContent;

            emit activationEmailResent(success);
        };

    const api::AccountEmail account{email.toStdString()};
    const auto accountManager = d->resendActivationConnection->accountManager();
    accountManager->reactivateAccount(account, nx::utils::guarded(this, callback));
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
    cloudPort(-1),
    credentials(),
    effectiveUserName(),
    stayConnected(false),
    errorCode(QnCloudStatusWatcher::NoError),
    updateTimer(new QTimer(this)),
    status(QnCloudStatusWatcher::LoggedOut),
    cloudSystems(),
    recentCloudSystems(qnClientCoreSettings->recentCloudSystems()),
    currentSystem(),
    temporaryCredentials(),
    m_pingTimer(new QTimer(this)),
    m_cloudIsEnabled(true)
{
    Q_Q(QnCloudStatusWatcher);

    updateTimer->setInterval(kUpdateIntervalMs);
    updateTimer->start();

    connect(updateTimer, &QTimer::timeout, q,
        [this, q]()
        {
             if (checkSuppressed())
                 q->updateSystems();
        });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
            this, &QnCloudStatusWatcherPrivate::updateCurrentSystem);

    connect(m_pingTimer, &QTimer::timeout, this,
        &QnCloudStatusWatcherPrivate::prolongTemporaryCredentials);
}

bool QnCloudStatusWatcherPrivate::cloudIsEnabled() const
{
    return m_cloudIsEnabled;
}

void QnCloudStatusWatcherPrivate::setCloudEnabled(bool enabled)
{
    Q_Q(QnCloudStatusWatcher);

    if (m_cloudIsEnabled == enabled)
        return;

    m_cloudIsEnabled = enabled;
    emit q->isCloudEnabledChanged();
}

void QnCloudStatusWatcherPrivate::updateConnection(bool initial)
{
    Q_Q(QnCloudStatusWatcher);

    const auto status = (initial
        ? QnCloudStatusWatcher::Offline
        : QnCloudStatusWatcher::LoggedOut);
    setStatus(status, QnCloudStatusWatcher::NoError);
    cloudConnection.reset();
    temporaryConnection.reset();

    if (!credentials.isValid())
    {
        const auto error = (initial || credentials.isEmpty())
            ? QnCloudStatusWatcher::NoError
            : credentials.user.isEmpty()
              ? QnCloudStatusWatcher::InvalidEmail
              : QnCloudStatusWatcher::InvalidPassword;
        setStatus(QnCloudStatusWatcher::LoggedOut, error);
        setCloudSystems(QnCloudSystemList());
        q->setEffectiveUserName(QString());
        return;
    }

    cloudConnection = qnCloudConnectionProvider->createConnection();
    cloudConnection->setCredentials(credentials.user.toStdString(),
        credentials.password.toStdString());

    /* Very simple email check. */
    if (credentials.user.contains(L'@'))
        q->setEffectiveUserName(credentials.user);
    else
        q->setEffectiveUserName(QString());

    updateCurrentAccount();
}

void QnCloudStatusWatcherPrivate::setStatus(QnCloudStatusWatcher::Status newStatus,
    QnCloudStatusWatcher::ErrorCode newErrorCode)
{
    const bool isNewStatus = (status != newStatus);
    const bool isNewErrorCode = (errorCode != newErrorCode);

    if (!isNewStatus && !isNewErrorCode)
        return;

    if (!stayConnected && (newStatus == QnCloudStatusWatcher::Offline))
        newStatus = QnCloudStatusWatcher::LoggedOut;

    // We have to update both values atomically
    status = newStatus;
    errorCode = newErrorCode;

    Q_Q(QnCloudStatusWatcher);
    // This should catch the case when user changes cloud password from browser.
    if (isNewStatus
        && status == QnCloudStatusWatcher::LoggedOut
        && errorCode == QnCloudStatusWatcher::InvalidPassword)
    {
        NX_ERROR(this, "Detected invalid user password. Forcing logout. Could it be user has changed the password?");
        q->resetCredentials(true);
        nx::vms::client::core::helpers::forgetSavedCloudCredentials(true);
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
    emit q->beforeCloudSystemsChanged(newCloudSystems);

    cloudSystems = newCloudSystems;

    emit q->cloudSystemsChanged(cloudSystems);

    updateCurrentSystem();
}

void QnCloudStatusWatcherPrivate::setRecentCloudSystems(const QnCloudSystemList &newRecentCloudSystems)
{
    if (recentCloudSystems == newRecentCloudSystems)
        return;

    recentCloudSystems = newRecentCloudSystems;

    Q_Q(QnCloudStatusWatcher);
    emit q->recentCloudSystemsChanged();
}

void QnCloudStatusWatcherPrivate::updateCurrentSystem()
{
    Q_Q(QnCloudStatusWatcher);

    const auto systemId = qnGlobalSettings->cloudSystemId();

    const auto it = std::find_if(
        cloudSystems.begin(), cloudSystems.end(),
        [systemId](const QnCloudSystem& system) { return systemId == system.cloudId; });

    if (it == cloudSystems.end())
        return;

    if (!it->visuallyEqual(currentSystem))
    {
        currentSystem = *it;
        emit q->currentSystemChanged(currentSystem);
    }
}

void QnCloudStatusWatcherPrivate::updateCurrentAccount()
{
    NX_ASSERT(cloudConnection);
    if (!cloudConnection)
        return;

    TRACE("Updating current account");
    auto callback = [this](api::ResultCode result, api::AccountData accountData)
        {
            QString value = QString::fromStdString(accountData.email);
            TRACE("Received effective username" << value);
            Q_Q(QnCloudStatusWatcher);
            q->setEffectiveUserName(value);

            updateStatusFromResultCode(result);
            if (result == api::ResultCode::ok)
            {
                createTemporaryCredentials();
                q->updateSystems();
            }
        };

    const auto guard = QPointer<QObject>(this);
    const auto thread = guard->thread();
    const auto completionHandler =
        [callback, guard, thread](api::ResultCode result, api::AccountData accountData)
        {
            if (!guard)
                return;

            const auto timerCallback =
                [callback, result, accountData, guard]()
                {
                    if (guard)
                        callback(result, accountData);
                };
            executeDelayed(timerCallback, 0, thread);
        };

    cloudConnection->accountManager()->getAccount(completionHandler);
}

void QnCloudStatusWatcherPrivate::createTemporaryCredentials()
{
    m_pingTimer->stop();
    NX_ASSERT(cloudConnection);
    if (!cloudConnection)
        return;

    TRACE("Creating temporary credentials");
    api::TemporaryCredentialsParams params;
#ifdef DEBUG_CLOUD_STATUS_WATCHER
    params.timeouts.autoProlongationEnabled = true;
    params.timeouts.expirationPeriod = std::chrono::seconds(30);
    params.timeouts.prolongationPeriod = std::chrono::seconds(10);
#else
    // TODO: #ak make this constant accessible through API
    params.type = std::string("short");
#endif
    auto callback = [this](api::ResultCode /*result*/, api::TemporaryCredentials credentials)
        {
            temporaryCredentials = credentials;
            /* Ping twice as often to make sure temporary credentials will stay alive. */
            const int keepAliveMs = std::chrono::milliseconds(
                temporaryCredentials.timeouts.prolongationPeriod).count() / 2;
            TRACE("Received temporary credentials, prolong after " << keepAliveMs);
            m_pingTimer->setInterval(keepAliveMs);
            m_pingTimer->start();
        };


    const auto guard = QPointer<QObject>(this);
    const auto thread = guard->thread();
    const auto completionHandler =
        [callback, guard, thread](api::ResultCode result, api::TemporaryCredentials credentials)
        {
            if (!guard)
                return;

            const auto timerCallback =
                [callback, result, credentials, guard]
                {
                    if (guard)
                        callback(result, credentials);
                };
            executeDelayed(timerCallback, 0, thread);
        };

    cloudConnection->accountManager()->createTemporaryCredentials(params, completionHandler);
}

void QnCloudStatusWatcherPrivate::prolongTemporaryCredentials()
{
    if (!checkSuppressed())
        return;

    TRACE("Prolong temporary credentials");
    if (temporaryCredentials.login.empty())
        return;

    if (!temporaryConnection)
    {
        TRACE("Creating new temporary connection");
        temporaryConnection = qnCloudConnectionProvider->createConnection();
        temporaryConnection->setCredentials(temporaryCredentials.login, temporaryCredentials.password);
    }

    NX_ASSERT(temporaryConnection);
    if (!temporaryConnection)
        return;

    auto callback = [this](api::ResultCode result)
        {
            TRACE("Ping result " << QString::fromStdString(api::toString(result)));
            if (result == api::ResultCode::ok)
                return;

            if (result == api::ResultCode::notAuthorized
                || result == api::ResultCode::credentialsRemovedPermanently)
            {
                TRACE("Temporary credentials are invalid, creating new");
                createTemporaryCredentials();
            }
        };

    TRACE("Ping...");
    const auto guard = QPointer<QObject>(this);
    const auto thread = guard->thread();
    const auto completionHandler =
        [callback, guard, thread](api::ResultCode result, api::ModuleInfo info)
        {
            Q_UNUSED(info);
            if (!guard)
                return;

            const auto timerCallback =
                [callback, result, guard]()
                {
                    if (guard)
                        callback(result);
                };
            executeDelayed(timerCallback, 0, thread);
        };

    temporaryConnection->ping(completionHandler);
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
