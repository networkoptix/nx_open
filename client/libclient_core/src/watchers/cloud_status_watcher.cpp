#include "cloud_status_watcher.h"

#include <chrono>
#include <algorithm>

#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtCore/QSettings>

#include <api/global_settings.h>

#include <client_core/client_core_settings.h>

#include <cloud/cloud_connection.h>

#include <utils/common/delayed.h>

#include <nx/utils/string.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/model_functions.h>

using namespace nx::cdb;

//#define DEBUG_CLOUD_STATUS_WATCHER
#ifdef DEBUG_CLOUD_STATUS_WATCHER
#define TRACE(...) qDebug() << "QnCloudStatusWatcher: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif

namespace {

const auto kCloudSystemJsonHolderTag = lit("json");

const int kUpdateIntervalMs = 5 * 1000;

QString getLocalSystemId(const std::string& opaque)
{
    QMap<QByteArray, QByteArray> params;
    nx::utils::parseNameValuePairs(QByteArray::fromStdString(opaque), ',', &params);

    static const auto kOpaqueLocalSystemIdTag = QByteArray("localSystemId");
    if (!params.contains(kOpaqueLocalSystemIdTag))
        return QString();

    return QString::fromUtf8(params.value(kOpaqueLocalSystemIdTag));
}

QnCloudSystemList getCloudSystemList(const api::SystemDataExList &systemsList)
{
    QnCloudSystemList result;

    for (const api::SystemDataEx &systemData : systemsList.systems)
    {
        if (systemData.status != api::SystemStatus::ssActivated)
            continue;

        QnCloudSystem system;
        system.cloudId = QString::fromStdString(systemData.id);
        system.localId = getLocalSystemId(systemData.opaque);
        if (system.localId.isEmpty())
            system.localId = system.cloudId;

        system.name = QString::fromStdString(systemData.name);
        system.ownerAccountEmail = QString::fromStdString(systemData.ownerAccountEmail);
        system.ownerFullName = QString::fromStdString(systemData.ownerFullName);
        system.authKey = systemData.authKey;
        system.weight = systemData.usageFrequency;
        system.lastLoginTimeUtcMs = std::chrono::duration_cast<std::chrono::milliseconds>
            (systemData.lastLoginTime.time_since_epoch()).count();
        result.append(system);
    }

    return result;
}

}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnCloudSystem), (json), _Fields)

class QnCloudStatusWatcherPrivate : public QObject
{
    QnCloudStatusWatcher* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusWatcher)

public:
    QnCloudStatusWatcherPrivate(QnCloudStatusWatcher* parent);

    std::string cloudHost;
    int cloudPort;
    QnCredentials credentials;
    QString effectiveUserName;
    bool stayConnected;
    QnCloudStatusWatcher::ErrorCode errorCode;

    QTimer *updateTimer;

    std::unique_ptr<api::Connection> cloudConnection;
    std::unique_ptr<api::Connection> temporaryConnection;

    QnCloudStatusWatcher::Status status;

    QnCloudSystemList cloudSystems;
    QnCloudSystemList recentCloudSystems;
    QnCloudSystem currentSystem;
    api::TemporaryCredentials temporaryCredentials;

public:
    void updateConnection(bool initial = false);

    void setCloudEnabled(bool enabled);
    bool cloudIsEnabled() const;
private:
    void setStatus(QnCloudStatusWatcher::Status newStatus,
        QnCloudStatusWatcher::ErrorCode error);
    void setCloudSystems(const QnCloudSystemList &newCloudSystems);
    void setRecentCloudSystems(const QnCloudSystemList &newRecentSystems);
    void updateCurrentSystem();
    void updateCurrentAccount();
    void createTemporaryCredentials();
    void prolongTemporaryCredentials();

private:
    QTimer* m_pingTimer;
    bool m_cloudIsEnabled;
};

QnCloudStatusWatcher::QnCloudStatusWatcher(QObject* parent):
    base_type(parent),
    d_ptr(new QnCloudStatusWatcherPrivate(this))
{
    setStayConnected(!qnClientCoreSettings->cloudPassword().isEmpty());

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
    connect(this, &QnCloudStatusWatcher::recentCloudSystemsChanged, this,
        [this]()
        {
            qnClientCoreSettings->setRecentCloudSystems(recentCloudSystems());
            qnClientCoreSettings->save();
        });

    //TODO: #GDM store temporary credentials
    setCloudCredentials(QnCredentials(
        qnClientCoreSettings->cloudLogin(), qnClientCoreSettings->cloudPassword()), true);

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int id)
        {
            if (id != QnClientCoreSettings::CloudPassword)
                return;

            setStayConnected(!qnClientCoreSettings->cloudPassword().isEmpty());
        });
}

QnCloudStatusWatcher::~QnCloudStatusWatcher()
{
}

QnCredentials QnCloudStatusWatcher::credentials() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->credentials;
}

void QnCloudStatusWatcher::setCredentials(const QnCredentials& value)
{
    Q_D(QnCloudStatusWatcher);
    if (d->credentials == value)
        return;

    bool isUserChanged = d->credentials.user != value.user;
    bool isPasswordChanged = d->credentials.password != value.password;

    d->credentials = value;
    d->updateConnection();

    if (isUserChanged)
        emit loginChanged();
    if (isPasswordChanged)
        emit passwordChanged();
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

            NX_LOG(lit("Error logging session %1: %2")
                .arg(cloudSystemId)
                .arg(QString::fromStdString(api::toString(result))),
                cl_logINFO);
        });
}

void QnCloudStatusWatcher::resetCloudCredentials()
{
    setCloudCredentials(QnCredentials());
}

void QnCloudStatusWatcher::setCloudCredentials(const QnCredentials& credentials, bool initial)
{
    Q_D(QnCloudStatusWatcher);

    if (d->credentials == credentials)
        return;

    d->credentials = credentials;

    d->updateConnection(initial);
    emit loginChanged();
    emit passwordChanged();
}

QnCredentials QnCloudStatusWatcher::createTemporaryCredentials() const
{
    Q_D(const QnCloudStatusWatcher);
    return QnCredentials(
        QString::fromStdString(d->temporaryCredentials.login),
        QString::fromStdString(d->temporaryCredentials.password));
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

void QnCloudStatusWatcher::updateSystems()
{
    Q_D(QnCloudStatusWatcher);

    if (!d->cloudConnection)
        return;

    QPointer<QnCloudStatusWatcher> guard(this);
    d->cloudConnection->systemManager()->getSystems(
        [this, guard](api::ResultCode result, const api::SystemDataExList &systemsList)
        {
            if (!guard)
                return;

            Q_D(QnCloudStatusWatcher);
            if (!d->cloudConnection)
                return;

            QnCloudSystemList cloudSystems;

            if (result == api::ResultCode::ok)
                cloudSystems = getCloudSystemList(systemsList);

            const auto handler =
                [this, guard, result, cloudSystems]()
                {
                    if (!guard)
                        return;

                    Q_D(QnCloudStatusWatcher);
                    if (!d->cloudConnection)
                        return;

                    d->setCloudEnabled((result != api::ResultCode::networkError)
                        && (result != api::ResultCode::serviceUnavailable));

                    switch (result)
                    {
                        case api::ResultCode::ok:
                            d->setCloudSystems(cloudSystems);
                            d->setStatus(QnCloudStatusWatcher::Online,
                                QnCloudStatusWatcher::NoError);
                            break;
                        case api::ResultCode::notAuthorized:
                            d->setStatus(QnCloudStatusWatcher::LoggedOut,
                                QnCloudStatusWatcher::InvalidCredentials);
                            break;
                        default:
                            d->setStatus(QnCloudStatusWatcher::Offline,
                                QnCloudStatusWatcher::UnknownError);
                            break;
                    }
            };

            executeDelayed(handler, 0, thread());
        }
    );
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
    connect(updateTimer, &QTimer::timeout, q, &QnCloudStatusWatcher::updateSystems);
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
            : QnCloudStatusWatcher::InvalidCredentials;
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
    createTemporaryCredentials();
    q->updateSystems();
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
    if (isNewStatus)
        emit q->statusChanged(status);

    if (isNewErrorCode && (errorCode != QnCloudStatusWatcher::NoError))
        emit q->errorChanged();
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
    auto callback = [this](api::ResultCode /*result*/, api::AccountData accountData)
        {
            QString value = QString::fromStdString(accountData.email);
            TRACE("Received effective username" << value);
            Q_Q(QnCloudStatusWatcher);
            q->setEffectiveUserName(value);
        };
    auto targetThread = QThread::currentThread();

    cloudConnection->accountManager()->getAccount(
        [callback, targetThread](api::ResultCode result, api::AccountData accountData)
    {
        executeDelayed([callback, result, accountData] { callback(result, accountData); }, 0, targetThread);
    });
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
    //TODO: #ak make this constant accessible through API
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
    auto targetThread = QThread::currentThread();

    cloudConnection->accountManager()->createTemporaryCredentials(params,
        [callback, targetThread](api::ResultCode result, api::TemporaryCredentials credentials)
        {
            executeDelayed([callback, result, credentials]{ callback(result, credentials); }, 0, targetThread);
        });
}

void QnCloudStatusWatcherPrivate::prolongTemporaryCredentials()
{
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

            if (result == api::ResultCode::notAuthorized)
            {
                TRACE("Temporary credentials are invalid, creating new");
                createTemporaryCredentials();
            }
        };
    auto targetThread = QThread::currentThread();

    TRACE("Ping...");
    temporaryConnection->ping(
        [callback, targetThread](api::ResultCode result, api::ModuleInfo info)
        {
            Q_UNUSED(info);
            executeDelayed([callback, result]{ callback(result); }, 0, targetThread);
        });
}

bool QnCloudSystem::operator ==(const QnCloudSystem &other) const
{
    return ((cloudId == other.cloudId)
        && (localId == other.localId)
        && (name == other.name)
        && (authKey == other.authKey)
        && (lastLoginTimeUtcMs == other.lastLoginTimeUtcMs)
        && qFuzzyEquals(weight, other.weight));
}

bool QnCloudSystem::visuallyEqual(const QnCloudSystem& other) const
{
    return (cloudId == other.cloudId
        && (localId == other.localId)
        && (name == other.name)
        && (authKey == other.authKey)
        && (ownerAccountEmail == other.ownerAccountEmail)
        && (ownerFullName == other.ownerFullName));
}

void QnCloudSystem::writeToSettings(QSettings* settings) const
{

    QByteArray json;
    QJson::serialize(*this, &json);
    const auto value = QVariant::fromValue(json);
    settings->setValue(kCloudSystemJsonHolderTag, value);
}

QnCloudSystem QnCloudSystem::fromSettings(QSettings* settings)
{
    QnCloudSystem result;
    const auto json = settings->value(kCloudSystemJsonHolderTag).toByteArray();
    QJson::deserialize(json, &result);
    return result;
}
