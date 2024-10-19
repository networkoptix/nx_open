// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQml/QJSValue>

#include <nx/utils/impl_ptr.h>

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::mobile {

/**
 * Class represents facade for all user interactions related to the push notifications.
 *
 * To receive push notifications user should allow them in OS and turn them on in the mobile client
 * settings. Client tries to turn on push notifications automatically when user logs in first time.
 * All settings are stored locally. When user enables push notfications application sends local
 * settings to the cloud.
 *
 * When notifications are turned on in client settings application sends subscription
 * request to the cloud instance. Within it client specifies requested systems and application
 * token. To disable notification client sends unsubscribe request with aplication token.
 */
class PushNotificationManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(Qt::CheckState enabledCheckState
        READ enabledCheckState
        NOTIFY enabledCheckStateChanged)

    Q_PROPERTY(bool expertMode
        READ expertMode
        NOTIFY expertModeChanged)

    Q_PROPERTY(bool userUpdateInProgress
        READ userUpdateInProgress
        NOTIFY userUpdateInProgressChanged)

    Q_PROPERTY(bool loggedIn
        READ loggedIn
        NOTIFY loggedInChanged)

    Q_PROPERTY(QStringList selectedSystems
        READ selectedSystems
        NOTIFY selectedSystemsChanged)

    Q_PROPERTY(bool hasOsPermission
        READ hasOsPermission
        NOTIFY hasOsPermissionChanged)

    Q_PROPERTY(int systemsCount
        READ systemsCount
        NOTIFY systemsCountChanged)

    Q_PROPERTY(int usedSystemsCount
        READ usedSystemsCount
        NOTIFY usedSystemsCountChanged)

public:
    static void registerQmlType();

    PushNotificationManager(QObject* parent = nullptr);
    virtual ~PushNotificationManager() override;

    /**
     * Sets cloud credentials to be used for registration for push notifications in cloud instance.
     */
    void setCredentials(const nx::network::http::Credentials& value);

    Qt::CheckState enabledCheckState() const;

    /**
     * Tries to set enabled state for notifications. To enable push notifications application
     * should register itself within the cloud instance by "subcribe" request.
     * If user tries to enable notifications and error occurs then state remains disabled.
     * If user tries to disable notifications and error occurs then state becomes disabled
     * anyway since we turn off pushes on the client side.
     */
    Q_INVOKABLE void setEnabled(bool value);

    /**
     * @return True if mode is "expert".
     */
    bool expertMode() const;

    /**
     * Explicitly sets simple mode for push notifications.
     * Simple mode allows user to receive notifications from all (including newly added in future)
     * systems.
     */
    Q_INVOKABLE void setSimpleMode(QJSValue callback = QJSValue());

    /**
     * Explicitly sets expert mode with specified systems.
     * Expert mode allows user to select specific systems to receive notifications.
     * @param systems List of system ids to receive notifications from.
     * @param callback Callback for operation
     */
    Q_INVOKABLE void setExpertMode(
        const QStringList& systems,
        QJSValue callback = QJSValue());

    /**
     * @return True if user operation (like mode/systems list) is in progress now.
     */
    bool userUpdateInProgress() const;

    /**
     * @return True if user logged in to the cloud and handled by push notificatioins manager.
     */
    bool loggedIn() const;

    /**
     * @return List of system ids:
     *     - empty list if user is not logged in;
     *     - nx::vms::client::mobile::details::allSystemsModeData() value in case of simple mode;
     *     - list of systems for which push notifications are enabled in case of expert mode.
     */
    QStringList selectedSystems() const;

    /**
     * Shows if user allows showing push notifcations in OS.
     */
    bool hasOsPermission() const;

    /**
     * Shows OS screen with push notification settings.
     */
    Q_INVOKABLE void showOsPushSettings();

    /**
     * Returns total systems count bound to the cloud account.
     */
    int systemsCount() const;

    /**
     * Updates cloud account systems list.
     * @param systems List of systems bound to the current cloud account
     */
    void setSystems(const QStringList& systems);

    /**
     * @return Number of systems selected for push notification receiving.
     */
    int usedSystemsCount() const;

    Q_INVOKABLE QString checkConnectionErrorText() const;

signals:
    void enabledCheckStateChanged();
    void expertModeChanged();
    void showPushErrorMessage(
        const QString& caption,
        const QString& description);
    void userUpdateInProgressChanged();
    void loggedInChanged();
    void selectedSystemsChanged();
    void hasOsPermissionChanged();
    void systemsCountChanged();
    void usedSystemsCountChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
