// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <QtCore/QObject>

#include <network/cloud_system_data.h>
#include <nx/cloud/db/api/account_data.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

class QnSystemDescription;
class CloudStatusWatcherPrivate;

namespace nx::vms::client::core {

struct CloudAuthData;

class NX_VMS_CLIENT_CORE_API CloudStatusWatcher:
    public QObject
{
    using Credentials = nx::network::http::Credentials;
    using base_type = QObject;

    Q_OBJECT
    Q_PROPERTY(Credentials credentials READ credentials NOTIFY credentialsChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(ErrorCode error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool isCloudEnabled READ isCloudEnabled NOTIFY isCloudEnabledChanged)
    Q_PROPERTY(QString cloudLogin READ cloudLogin NOTIFY cloudLoginChanged)

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

    explicit CloudStatusWatcher(QObject* parent = nullptr);
    virtual ~CloudStatusWatcher() override;

    // Username and access token with global scope.
    Credentials credentials() const;
    // Special credentials for RemoteConnectionFactory.
    Credentials remoteConnectionCredentials() const;

    /** Resets current cloud credentials. */
    Q_INVOKABLE void resetAuthData();

    bool setInitialAuthData(const nx::vms::client::core::CloudAuthData& authData);
    bool setAuthData(const nx::vms::client::core::CloudAuthData& authData, bool forced = false);

    QString cloudLogin() const;
    bool is2FaEnabledForUser() const;

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

    Status status() const;
    ErrorCode error() const;
    bool isCloudEnabled() const;

    void updateSystems();
    std::optional<QnCloudSystem> cloudSystem(const QString& systemId) const;
    QnCloudSystemList cloudSystems() const;
    QnCloudSystemList recentCloudSystems() const;

    Q_INVOKABLE void resendActivationEmail(const QString& email);

signals:
    void activationEmailResent(bool success);
    void credentialsChanged();
    void cloudLoginChanged();
    void forcedLogout();
    void loggedOutWithError();
    void statusChanged(Status status);
    void errorChanged(ErrorCode error);
    void isCloudEnabledChanged();
    void is2FaEnabledForUserChanged();

    void cloudSystemsChanged(const QnCloudSystemList& currentCloudSystems);
    void recentCloudSystemsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

#define qnCloudStatusWatcher appContext()->cloudStatusWatcher()
