#pragma once

#include <QtCore/QObject>

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

signals:
    void statusChanged(Status status);
    void error(ErrorCode errorCode);

private:
    QScopedPointer<QnCloudStatusWatcherPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusWatcher)
};
