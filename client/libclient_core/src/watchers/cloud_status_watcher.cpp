#include "cloud_status_watcher.h"

#include <chrono>
#include <algorithm>

#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtCore/QSettings>

#include <api/global_settings.h>
#include <cdb/connection.h>
#include <utils/common/delayed.h>
#include <client_core/client_core_settings.h>

using namespace nx::cdb;

//#define DEBUG_CLOUD_STATUS_WATCHER
#ifdef DEBUG_CLOUD_STATUS_WATCHER
#define TRACE(...) qDebug() << "QnCloudStatusWatcher: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif


namespace {

const auto kIdTag = lit("id");
const auto kNameTag = lit("name");
const auto kOwnerAccounEmail = lit("email");
const auto kOwnerFullName = lit("owner_full_name");
const auto kWeight = lit("weight");
const auto kLastLoginTimeUtcMs = lit("kLastLoginTimeUtcMs");

const int kUpdateIntervalMs = 5 * 1000;

QnCloudSystemList getCloudSystemList(const api::SystemDataExList &systemsList)
{
    QnCloudSystemList result;

    for (const api::SystemDataEx &systemData : systemsList.systems)
    {
        if (systemData.status != api::SystemStatus::ssActivated)
            continue;

        QnCloudSystem system;
        system.id = QString::fromStdString(systemData.id);
        system.name = QString::fromStdString(systemData.name);
        system.ownerAccountEmail = QString::fromStdString(systemData.ownerAccountEmail);
        system.ownerFullName = QString::fromStdString(systemData.ownerFullName);
        system.authKey = systemData.authKey;
        system.weight = systemData.usageFrequency;
        system.lastLoginTimeUtcMs = std::chrono::duration_cast<std::chrono::milliseconds>
            (systemData.lastLoginTime.time_since_epoch()).count();
        result.append(system);
    }

    {
        // TODO: #ynikitenkov remove this section when weights are available

        // Temporary code section.
        const bool isTmpValues = std::all_of(result.begin(), result.end(),
            [](const QnCloudSystem& system) -> bool { return !system.weight; });
        if (isTmpValues)
        {
            static const auto initialWeight = 10000.0;
            static const auto step = 100.0;

            qreal tmpWeight = initialWeight;
            const auto tmpLastLoginTime = QDateTime::currentMSecsSinceEpoch();
            for (auto& system : result)
            {
                system.weight = tmpWeight;
                system.lastLoginTimeUtcMs = tmpLastLoginTime;
                tmpWeight += step;
            }
        }
    }

    return result;
}

}

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

    std::unique_ptr<api::ConnectionFactory, decltype(&destroyConnectionFactory)> connectionFactory;
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
        });

    setCloudEndpoint(qnClientCoreSettings->cdbEndpoint());
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

QString QnCloudStatusWatcher::cloudEndpoint() const
{
    Q_D(const QnCloudStatusWatcher);
    return lit("%1:%2").arg(QString::fromStdString(d->cloudHost)).arg(d->cloudPort);
}

void QnCloudStatusWatcher::setCloudEndpoint(const QString &endpoint)
{
    Q_D(QnCloudStatusWatcher);

    QUrl url = QUrl::fromUserInput(endpoint);

    if (!url.isValid())
        return;

    d->cloudHost = url.host().toStdString();
    d->cloudPort = url.port();

    if (!d->cloudHost.empty() || d->cloudPort <= 0)
        return;

    d->connectionFactory->setCloudEndpoint(d->cloudHost, d->cloudPort);

    d->updateConnection();
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
    connectionFactory(createConnectionFactory(), &destroyConnectionFactory),
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

    connect(m_pingTimer, &QTimer::timeout, this, &QnCloudStatusWatcherPrivate::prolongTemporaryCredentials);
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

    cloudConnection = connectionFactory->createConnection();
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

    cloudSystems = newCloudSystems;

    Q_Q(QnCloudStatusWatcher);
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

    const auto systemId = qnGlobalSettings->cloudSystemID();

    const auto it = std::find_if(
        cloudSystems.begin(), cloudSystems.end(),
        [systemId](const QnCloudSystem& system) { return systemId == system.id; });

    if (it == cloudSystems.end())
        return;

    if (!it->fullEqual(currentSystem))
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
        temporaryConnection = connectionFactory->createConnection();
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
    return id == other.id &&
           name == other.name &&
           authKey == other.authKey;
}

bool QnCloudSystem::fullEqual(const QnCloudSystem& other) const
{
    return id == other.id &&
           name == other.name &&
           authKey == other.authKey &&
           ownerAccountEmail == other.ownerAccountEmail &&
           ownerFullName == other.ownerFullName;
}

void QnCloudSystem::writeToSettings(QSettings* settings) const
{
    settings->setValue(kIdTag, id);
    settings->setValue(kNameTag, name);
    settings->setValue(kOwnerAccounEmail, ownerAccountEmail);
    settings->setValue(kOwnerFullName, ownerFullName);
    settings->setValue(kWeight, weight);
    settings->setValue(kLastLoginTimeUtcMs, lastLoginTimeUtcMs);
}

QnCloudSystem QnCloudSystem::fromSettings(QSettings* settings)
{
    QnCloudSystem result;

    result.id = settings->value(kIdTag).toString();
    result.name = settings->value(kNameTag).toString();
    result.ownerAccountEmail = settings->value(kOwnerAccounEmail).toString();
    result.ownerFullName = settings->value(kOwnerFullName).toString();
    result.weight = settings->value(kWeight, 0.0).toReal();
    result.lastLoginTimeUtcMs = settings->value(kLastLoginTimeUtcMs, 0).toLongLong();
    return result;
}
