#include "cloud_status_watcher.h"

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


namespace
{
    const auto kIdTag = lit("id");
    const auto kNameTag = lit("name");
    const auto kOwnerAccounEmail = lit("email");
    const auto kOwnerFullName = lit("owner_full_name");

    const int kUpdateInterval = 5 * 1000;

    QnCloudSystemList getCloudSystemList(const api::SystemDataExList &systemsList)
    {
        QnCloudSystemList result;

        for (const api::SystemDataEx &systemData : systemsList.systems)
        {
            if (systemData.status != api::ssActivated)
                continue;

            QnCloudSystem system;
            system.id = QString::fromStdString(systemData.id);
            system.name = QString::fromStdString(systemData.name);
            system.ownerAccountEmail = QString::fromStdString(systemData.ownerAccountEmail);
            system.ownerFullName = QString::fromStdString(systemData.ownerFullName);
            system.authKey = systemData.authKey;
            result.append(system);
        }

        std::sort(result.begin(), result.end());

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
    QString cloudLogin;
    QString cloudPassword;
    bool stayConnected;
    QnCloudStatusWatcher::ErrorCode errorCode;

    QTimer *updateTimer;

    std::unique_ptr<
        api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;
    std::unique_ptr<api::Connection> temporaryConnection;

    QnCloudStatusWatcher::Status status;
    bool loggedIn;

    QnCloudSystemList cloudSystems;
    QnCloudSystemList recentCloudSystems;
    QnCloudSystem currentSystem;
    api::TemporaryCredentials temporaryCredentials;

public:
    void updateConnection(bool initial = false);

private:
    void setStatus(QnCloudStatusWatcher::Status newStatus,
        QnCloudStatusWatcher::ErrorCode error);
    void setCloudSystems(const QnCloudSystemList &newCloudSystems);
    void setRecentCloudSystems(const QnCloudSystemList &newRecentSystems);
    void updateCurrentSystem();
    void createTemporaryCredentials();
    void prolongTemporaryCredentials();

private:
    QTimer* m_pingTimer;
};

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

}

QnCloudStatusWatcher::~QnCloudStatusWatcher()
{
}

QString QnCloudStatusWatcher::cloudLogin() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->cloudLogin;
}

void QnCloudStatusWatcher::setCloudLogin(const QString &login)
{
    Q_D(QnCloudStatusWatcher);

    if (d->cloudLogin == login)
        return;

    d->cloudLogin = login;
    d->updateConnection();
    emit loginChanged();
}

QString QnCloudStatusWatcher::cloudPassword() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->cloudPassword;
}

void QnCloudStatusWatcher::setCloudPassword(const QString &password)
{
    Q_D(QnCloudStatusWatcher);
    d->cloudPassword = password;
    d->updateConnection();
    emit passwordChanged();
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

void QnCloudStatusWatcher::setCloudCredentials(const QString &login, const QString &password, bool initial)
{
    Q_D(QnCloudStatusWatcher);

    if (d->cloudLogin == login && d->cloudPassword == password)
        return;

    d->cloudLogin = login;
    d->cloudPassword = password;

    d->updateConnection(initial);
    emit loginChanged();
    emit passwordChanged();
}

QString QnCloudStatusWatcher::temporaryLogin() const
{
    Q_D(const QnCloudStatusWatcher);
    return QString::fromStdString(d->temporaryCredentials.login);
}

QString QnCloudStatusWatcher::temporaryPassword() const
{
    Q_D(const QnCloudStatusWatcher);
    return QString::fromStdString(d->temporaryCredentials.password);
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

            QnCloudSystemList cloudSystems;

            if (result == api::ResultCode::ok)
                cloudSystems = getCloudSystemList(systemsList);

            const auto handler =
                [this, guard, result, cloudSystems]()
                {
                    if (!guard)
                        return;

                    Q_D(QnCloudStatusWatcher);

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
    cloudLogin(),
    cloudPassword(),
    stayConnected(false),
    errorCode(QnCloudStatusWatcher::NoError),
    updateTimer(new QTimer(this)),
    connectionFactory(createConnectionFactory(), &destroyConnectionFactory),
    status(QnCloudStatusWatcher::LoggedOut),
    cloudSystems(),
    recentCloudSystems(qnClientCoreSettings->recentCloudSystems()),
    currentSystem(),
    temporaryCredentials(),
    m_pingTimer(new QTimer(this))
{
    Q_Q(QnCloudStatusWatcher);

    updateTimer->setInterval(kUpdateInterval);
    updateTimer->start();
    connect(updateTimer, &QTimer::timeout, q, &QnCloudStatusWatcher::updateSystems);
    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
            this, &QnCloudStatusWatcherPrivate::updateCurrentSystem);

    connect(m_pingTimer, &QTimer::timeout, this, &QnCloudStatusWatcherPrivate::prolongTemporaryCredentials);
}

void QnCloudStatusWatcherPrivate::updateConnection(bool initial)
{
    const auto status = (initial
        ? QnCloudStatusWatcher::Offline
        : QnCloudStatusWatcher::LoggedOut);
    setStatus(status, QnCloudStatusWatcher::NoError);

    cloudConnection.reset();
    temporaryConnection.reset();

    if (cloudLogin.isEmpty() || cloudPassword.isEmpty())
    {
        const auto error = (initial
            ? QnCloudStatusWatcher::NoError
            : QnCloudStatusWatcher::InvalidCredentials);
        setStatus(QnCloudStatusWatcher::LoggedOut, error);
        setCloudSystems(QnCloudSystemList());
        return;
    }

    cloudConnection = connectionFactory->createConnection(cloudLogin.toStdString(), cloudPassword.toStdString());
    createTemporaryCredentials();

    Q_Q(QnCloudStatusWatcher);
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
    auto callback = [this](api::ResultCode result, api::TemporaryCredentials credentials)
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
        temporaryConnection = connectionFactory->createConnection(
            temporaryCredentials.login,
            temporaryCredentials.password);
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

bool QnCloudSystem::operator <(const QnCloudSystem &other) const
{
    int comp = name.compare(other.name);
    if (comp != 0)
        return comp < 0;

    return id < other.id;
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

void QnCloudSystem::writeToSettings(QSettings* settings, const QnCloudSystem& data)
{
    settings->setValue(kIdTag, data.id);
    settings->setValue(kNameTag, data.name);
    settings->setValue(kOwnerAccounEmail, data.ownerAccountEmail);
    settings->setValue(kOwnerFullName, data.ownerFullName);
}

QnCloudSystem QnCloudSystem::fromSettings(QSettings* settings)
{
    QnCloudSystem result;

    result.id = settings->value(kIdTag).toString();
    result.name = settings->value(kNameTag).toString();
    result.ownerAccountEmail = settings->value(kOwnerAccounEmail).toString();
    result.ownerFullName = settings->value(kOwnerFullName).toString();

    return result;
}
