#include "cloud_status_watcher.h"

#include <algorithm>

#include <QtCore/QUrl>
#include <QtCore/QPointer>

#include <cdb/connection.h>
#include <utils/common/delayed.h>

using namespace nx::cdb;

namespace
{
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
            system.authKey = systemData.authKey;
            result.append(system);
        }

        std::sort(result.begin(), result.end());

        return result;
    }
}

class QnCloudStatusWatcherPrivate : public QObject
{
    QnCloudStatusWatcher *q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusWatcher)

public:
    QnCloudStatusWatcherPrivate(QnCloudStatusWatcher *parent);

    std::string cloudHost;
    int cloudPort;
    QString cloudLogin;
    QString cloudPassword;

    QTimer *updateTimer;

    std::unique_ptr<
        api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;

    QnCloudStatusWatcher::Status status;
    bool loggedIn;

    QnCloudSystemList cloudSystems;

public:
    void updateConnection(bool initial = false);

private:
    void setStatus(QnCloudStatusWatcher::Status newStatus);
    void setCloudSystems(const QnCloudSystemList &newCloudSystems);
    void checkAndSetStatus(QnCloudStatusWatcher::Status newStatus);
};

QnCloudStatusWatcher::QnCloudStatusWatcher(QObject *parent)
    : base_type(parent)
    , d_ptr(new QnCloudStatusWatcherPrivate(this))
{
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
}

void QnCloudStatusWatcher::setCloudCredentials(const QString &login, const QString &password, bool initial)
{
    Q_D(QnCloudStatusWatcher);

    if (d->cloudLogin == login && d->cloudPassword == password)
        return;

    setCloudLogin(login);
    d->cloudPassword = password;

    d->updateConnection(initial);
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

                executeDelayed(
                        [this, guard, result, cloudSystems]()
                        {
                            if (!guard)
                                return;

                            Q_D(QnCloudStatusWatcher);

                            switch (result)
                            {
                            case api::ResultCode::ok:
                                d->setCloudSystems(cloudSystems);
                                d->checkAndSetStatus(QnCloudStatusWatcher::Online);
                                break;
                            case api::ResultCode::notAuthorized:
                                d->checkAndSetStatus(QnCloudStatusWatcher::Unauthorized);
                                emit error(QnCloudStatusWatcher::InvalidCredentials);
                                break;
                            default:
                                d->checkAndSetStatus(QnCloudStatusWatcher::Offline);
                                emit error(QnCloudStatusWatcher::UnknownError);
                                break;
                            }
                        },
                        0, thread()
                );
            }
    );
}

QnCloudSystemList QnCloudStatusWatcher::cloudSystems() const
{
    Q_D(const QnCloudStatusWatcher);
    return d->cloudSystems;
}

QnCloudStatusWatcherPrivate::QnCloudStatusWatcherPrivate(QnCloudStatusWatcher *parent)
    : QObject(parent)
    , q_ptr(parent)
    , cloudPort(-1)
    , updateTimer(new QTimer(this))
    , connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
    , status(QnCloudStatusWatcher::LoggedOut)
{
    Q_Q(QnCloudStatusWatcher);

    updateTimer->setInterval(kUpdateInterval);
    updateTimer->start();
    connect(updateTimer, &QTimer::timeout, q, &QnCloudStatusWatcher::updateSystems);
}

void QnCloudStatusWatcherPrivate::updateConnection(bool initial)
{
    checkAndSetStatus(initial ? QnCloudStatusWatcher::Offline : QnCloudStatusWatcher::LoggedOut);

    cloudConnection.reset();

    if (cloudLogin.isEmpty() || cloudPassword.isEmpty())
    {
        setCloudSystems(QnCloudSystemList());
        return;
    }

    cloudConnection = connectionFactory->createConnection(cloudLogin.toStdString(), cloudPassword.toStdString());

    Q_Q(QnCloudStatusWatcher);
    q->updateSystems();
}

void QnCloudStatusWatcherPrivate::setStatus(QnCloudStatusWatcher::Status newStatus)
{
    if (status == newStatus)
        return;

    status = newStatus;

    Q_Q(QnCloudStatusWatcher);
    emit q->statusChanged(status);
}

void QnCloudStatusWatcherPrivate::setCloudSystems(const QnCloudSystemList &newCloudSystems)
{
    if (cloudSystems == newCloudSystems)
        return;

    cloudSystems = newCloudSystems;

    Q_Q(QnCloudStatusWatcher);
    emit q->cloudSystemsChanged(cloudSystems);
}

void QnCloudStatusWatcherPrivate::checkAndSetStatus(QnCloudStatusWatcher::Status newStatus)
{
    switch (newStatus)
    {
    case QnCloudStatusWatcher::Online:
    case QnCloudStatusWatcher::LoggedOut:
        setStatus(newStatus);
        break;
    case QnCloudStatusWatcher::Unauthorized:
    case QnCloudStatusWatcher::Offline:
        if (newStatus == QnCloudStatusWatcher::LoggedOut)
            setStatus(QnCloudStatusWatcher::LoggedOut);
        else
            setStatus(newStatus);
        break;
    }
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
