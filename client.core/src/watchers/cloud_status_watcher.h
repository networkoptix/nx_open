#pragma once

#include <string>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

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

class QnCloudStatusWatcher : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:

    enum ErrorCode
    {
        InvalidCredentials,
        UnknownError
    };

    enum Status
    {
        LoggedOut,
        Online,
        Offline,
        Unauthorized
    };

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
