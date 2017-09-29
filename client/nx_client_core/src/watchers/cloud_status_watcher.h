#pragma once

#include <string>

#include <QtCore/QObject>

#include <common/common_module_aware.h>

#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>

#include <utils/common/encoded_credentials.h>
#include <network/cloud_system_data.h>

class QSettings;
class QnSystemDescription;
class QnCloudStatusWatcherPrivate;

class QnCloudStatusWatcher : public QObject, public Singleton<QnCloudStatusWatcher>, public QnCommonModuleAware
{
    Q_OBJECT
    Q_PROPERTY(QnEncodedCredentials credentials READ credentials NOTIFY credentialsChanged)
    Q_PROPERTY(QString effectiveUserName READ effectiveUserName NOTIFY effectiveUserNameChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(ErrorCode error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool stayConnected READ stayConnected WRITE setStayConnected NOTIFY stayConnectedChanged)
    Q_PROPERTY(bool isCloudEnabled READ isCloudEnabled NOTIFY isCloudEnabledChanged)

    using base_type = QObject;

public:
    enum ErrorCode
    {
        NoError,
        InvalidCredentials,
        AccountNotActivated,
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

    explicit QnCloudStatusWatcher(QObject* parent = nullptr, bool isMobile = true);
    virtual ~QnCloudStatusWatcher() override;

    QnEncodedCredentials credentials() const;
    void resetCredentials();
    void setCredentials(const QnEncodedCredentials& credentials, bool initial = false);

    // These getters are for qml
    Q_INVOKABLE QString cloudLogin() const;
    Q_INVOKABLE QString cloudPassword() const;

    QString effectiveUserName() const;
    void setEffectiveUserName(const QString& value);

    bool stayConnected() const;
    void setStayConnected(bool value);

    void logSession(const QString& cloudSystemId);

    /**
     * Get temporary credentials for one-time use. Fast sequential calls will get the same result.
     */
    QnEncodedCredentials createTemporaryCredentials();

    Status status() const;

    ErrorCode error() const;

    bool isCloudEnabled() const;

    void updateSystems();

    QnCloudSystemList cloudSystems() const;

    QnCloudSystemList recentCloudSystems() const;

signals:
    void credentialsChanged();
    void loginChanged();
    void passwordChanged();
    void effectiveUserNameChanged();
    void statusChanged(Status status);
    void beforeCloudSystemsChanged(const QnCloudSystemList &newCloudSystems);
    void cloudSystemsChanged(const QnCloudSystemList &currectCloudSystems);
    void recentCloudSystemsChanged();
    void currentSystemChanged(const QnCloudSystem& system);
    void errorChanged(ErrorCode error);
    void stayConnectedChanged();
    void isCloudEnabledChanged();

private:
    QScopedPointer<QnCloudStatusWatcherPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusWatcher)
};

#define qnCloudStatusWatcher QnCloudStatusWatcher::instance()
