#pragma once

#include <string>
#include <chrono>

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <utils/common/credentials.h>

#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>

#include <network/cloud_system_data.h>

class QSettings;
class QnSystemDescription;
class QnCloudStatusWatcherPrivate;

class QnCloudStatusWatcher:
    public QObject, 
    public Singleton<QnCloudStatusWatcher>,
    public QnCommonModuleAware
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::common::Credentials credentials READ credentials NOTIFY credentialsChanged)
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
        InvalidEmail,
        InvalidPassword,
        UserTemporaryLockedOut,
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

    nx::vms::common::Credentials credentials() const;
    /**
     * Resets current cloud credentials.
     * @param keepUser - should we keep username and reset only a password.
     */
    void resetCredentials(bool keepUser = false);
    bool setCredentials(const nx::vms::common::Credentials& credentials, bool initial = false);

    // These getters are for qml
    Q_INVOKABLE QString cloudLogin() const;
    Q_INVOKABLE QString cloudPassword() const;

    QString effectiveUserName() const;
    void setEffectiveUserName(const QString& value);

    bool stayConnected() const;
    void setStayConnected(bool value);

    void logSession(const QString& cloudSystemId);

    using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    /**
     * @brief Stops watcher from interacting with cloud from background
     * Background interaction with the cloud breaks user lockout feature in
     * ConnectToCloudDialog. Any request from the watcher is done with proper
     * user credentials, so it resets the counter for failed password attempts.
     * @param timeout: time period in milliseconds. Interaction will be
     *  automatically resumed after it expires.
     */
    void suppressCloudInteraction(TimePoint::duration timeout);
    /**
     * @brief Resumes background interaction with the cloud.
     */
    void resumeCloudInteraction();

    /**
     * Get temporary credentials for one-time use. Fast sequential calls will get the same result.
     */
    nx::vms::common::Credentials createTemporaryCredentials();

    Status status() const;

    ErrorCode error() const;

    bool isCloudEnabled() const;

    void updateSystems();

    QnCloudSystemList cloudSystems() const;

    QnCloudSystemList recentCloudSystems() const;

    Q_INVOKABLE void resendActivationEmail(const QString& email);

signals:
    void activationEmailResent(bool success);
    void credentialsChanged();
    void loginChanged();
    void passwordChanged();
    void forcedLogout();
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
