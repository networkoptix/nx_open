#pragma once

#include <string>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>

struct QnCloudSystem
{
    QString id;
    QString name;
    QString ownerAccountEmail;
    QString ownerFullName;
    std::string authKey;

    bool operator <(const QnCloudSystem &other) const;
    bool operator ==(const QnCloudSystem &other) const;
};

typedef QList<QnCloudSystem> QnCloudSystemList;

class QnCloudStatusWatcherPrivate;

class QnCloudStatusWatcher : public QObject, public Singleton<QnCloudStatusWatcher>
{
    Q_OBJECT
    Q_PROPERTY(QString cloudLogin READ cloudLogin WRITE setCloudLogin NOTIFY loginChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    using base_type = QObject;

public:

    enum ErrorCode
    {
        InvalidCredentials,
        UnknownError
    };
    Q_ENUM(ErrorCode)

    enum Status
    {
        LoggedOut,
        Online,
        Offline,
        Unauthorized
    };
    Q_ENUM(Status)

    explicit QnCloudStatusWatcher(QObject *parent = nullptr);
    ~QnCloudStatusWatcher();

    QString cloudLogin() const;
    void setCloudLogin(const QString &login);

    QString cloudPassword() const;
    void setCloudPassword(const QString &password);

    void setCloudCredentials(const QString &login, const QString &password, bool initial = false);

    QString cloudEndpoint() const;
    void setCloudEndpoint(const QString &endpoint);

    Status status() const;

    void updateSystems();

    QnCloudSystemList cloudSystems() const;

signals:
    void loginChanged();
    void statusChanged(Status status);
    void cloudSystemsChanged(const QnCloudSystemList &cloudSystems);
    void error(ErrorCode errorCode);

private:
    QScopedPointer<QnCloudStatusWatcherPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusWatcher)
};

#define qnCloudStatusWatcher QnCloudStatusWatcher::instance()
