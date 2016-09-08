#pragma once

#include <string>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>

#include <utils/common/credentials.h>

class QSettings;

struct QnCloudSystem
{
    QString id;
    QString name;
    QString ownerAccountEmail;
    QString ownerFullName;
    std::string authKey;

    bool operator <(const QnCloudSystem &other) const;
    bool operator ==(const QnCloudSystem &other) const;

    bool fullEqual(const QnCloudSystem& other) const;

    static void writeToSettings(QSettings* settings, const QnCloudSystem& data);

    static QnCloudSystem fromSettings(QSettings* settings);
};

typedef QList<QnCloudSystem> QnCloudSystemList;
Q_DECLARE_METATYPE(QnCloudSystemList);

class QnCloudStatusWatcherPrivate;

class QnCloudStatusWatcher : public QObject, public Singleton<QnCloudStatusWatcher>
{
    Q_OBJECT
    Q_PROPERTY(QString effectiveUserName READ effectiveUserName WRITE setEffectiveUserName NOTIFY effectiveUserNameChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(ErrorCode error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool stayConnected READ stayConnected WRITE setStayConnected NOTIFY stayConnectedChanged)

    using base_type = QObject;

public:

    enum ErrorCode
    {
        NoError,
        InvalidCredentials,
        UnknownError
    };
    Q_ENUM(ErrorCode)

    enum Status
    {
        LoggedOut,
        Online,
        Offline,        //< User is logged in with "stay connected" checked
                        // but internet connection is lost
    };
    Q_ENUM(Status)

    explicit QnCloudStatusWatcher(QObject *parent = nullptr);
    ~QnCloudStatusWatcher();

    QnCredentials credentials() const;
    void setCredentials(const QnCredentials& value);

    // These getters are for qml
    Q_INVOKABLE QString cloudLogin() const;
    Q_INVOKABLE QString cloudPassword() const;

    QString effectiveUserName() const;
    void setEffectiveUserName(const QString& value);

    bool stayConnected() const;
    void setStayConnected(bool value);

    void resetCloudCredentials();
    void setCloudCredentials(const QnCredentials& credentials, bool initial = false);

    QnCredentials createTemporaryCredentials() const;

    QString cloudEndpoint() const;
    void setCloudEndpoint(const QString &endpoint);

    Status status() const;

    ErrorCode error() const;

    void updateSystems();

    QnCloudSystemList cloudSystems() const;

    QnCloudSystemList recentCloudSystems() const;

signals:
    void loginChanged();
    void passwordChanged();
    void effectiveUserNameChanged();
    void statusChanged(Status status);
    void cloudSystemsChanged(const QnCloudSystemList &cloudSystems);
    void recentCloudSystemsChanged();
    void currentSystemChanged(const QnCloudSystem& system);
    void errorChanged();
    void stayConnectedChanged();

private:
    QScopedPointer<QnCloudStatusWatcherPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusWatcher)
};

#define qnCloudStatusWatcher QnCloudStatusWatcher::instance()
