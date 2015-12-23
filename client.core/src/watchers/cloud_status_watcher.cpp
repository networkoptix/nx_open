#include "cloud_status_watcher.h"

#include <QtCore/QUrl>

#include <cdb/connection.h>
#include <utils/common/delayed.h>

using namespace nx::cdb;

namespace
{
    const int kPingInterval = 30 * 1000;
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

    QTimer *pingTimer;

    std::unique_ptr<
        api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
    std::unique_ptr<api::Connection> cloudConnection;

    QnCloudStatusWatcher::Status status;
    bool loggedIn;

public:
    void updateConnection(bool initial = false);

private:
    void setStatus(QnCloudStatusWatcher::Status newStatus);
    void checkAndSetStatus(QnCloudStatusWatcher::Status newStatus);
    void pingCloud();
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
    d->cloudLogin = login;
    d->updateConnection();
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

    d->cloudLogin = login;
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

QnCloudStatusWatcherPrivate::QnCloudStatusWatcherPrivate(QnCloudStatusWatcher *parent)
    : QObject(parent)
    , q_ptr(parent)
    , cloudPort(-1)
    , pingTimer(new QTimer(this))
    , connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
    , status(QnCloudStatusWatcher::LoggedOut)
{
    pingTimer->setInterval(kPingInterval);
    connect(pingTimer, &QTimer::timeout, this, &QnCloudStatusWatcherPrivate::pingCloud);
}

void QnCloudStatusWatcherPrivate::updateConnection(bool initial)
{
    checkAndSetStatus(initial ? QnCloudStatusWatcher::Offline : QnCloudStatusWatcher::LoggedOut);

    cloudConnection.reset();

    if (cloudLogin.isEmpty() || cloudPassword.isEmpty())
        return;

    cloudConnection = connectionFactory->createConnection(cloudLogin.toStdString(), cloudPassword.toStdString());

    pingTimer->start();
    pingCloud();
}

void QnCloudStatusWatcherPrivate::setStatus(QnCloudStatusWatcher::Status newStatus)
{
    if (status == newStatus)
        return;

    status = newStatus;

    Q_Q(QnCloudStatusWatcher);
    emit q->statusChanged(status);
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

void QnCloudStatusWatcherPrivate::pingCloud()
{
    if (!cloudConnection)
        return;

    cloudConnection->ping([this](api::ResultCode result, api::ModuleInfo)
    {
        executeDelayed([this, result]()
                {
                    Q_Q(QnCloudStatusWatcher);

                    switch (result)
                    {
                    case api::ResultCode::ok:
                        checkAndSetStatus(QnCloudStatusWatcher::Online);
                        break;
                    case api::ResultCode::notAuthorized:
                        checkAndSetStatus(QnCloudStatusWatcher::Unauthorized);
                        emit q->error(QnCloudStatusWatcher::InvalidCredentials);
                        break;
                    default:
                        checkAndSetStatus(QnCloudStatusWatcher::Offline);
                        emit q->error(QnCloudStatusWatcher::UnknownError);
                        break;
                    }
                },
                0, thread()
        );
    });
}
