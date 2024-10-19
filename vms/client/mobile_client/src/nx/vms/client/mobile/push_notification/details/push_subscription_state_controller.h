// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "push_notification_structures.h"

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::mobile::details {

/**
 * Class manages remote push notification settings.
 * Since we store settings locally we need to updated them on the cloud only when they are changed.
 * Settings are considered confirmed when they are successfuly sent to the cloud instance.
 * Once set, we don't change settings between client restarts.
 */
class PushSettingsRemoteController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool userUpdateInProgress
        READ userUpdateInProgress
        NOTIFY userUpdateInProgressChanged)

    Q_PROPERTY(bool loggedIn
        READ loggedIn
        NOTIFY loggedInChanged)

public:
    PushSettingsRemoteController(QObject* parent = nullptr);

    virtual ~PushSettingsRemoteController() override;

    /**
     * @return True if user operation (like mode/systems list) is in progress now.
     */
    bool userUpdateInProgress() const;

    /**
     * @return Credentials for applying settings.
     */
    const nx::network::http::Credentials& credentials() const;

    /**
     * @return Settings which are applied at the cloud instance.
     */
    OptionalLocalPushSettings confirmedSettings() const;

    using Callback = std::function<void (bool success)>;

    /**
     * Manages push notifications settings at the remote side by sending the specified settings to
     * the cloud instance if needed. If there are no confirmed settings specified, tries to enable
     * push notifications for all systems. Sends a subscription request to the cloud instance every
     * time it's called.
     * time it called.
     * @param credentials Credentials for settings applying.
     * @param confirmedSettings Optional settings. Empty settings mean subscription for
     *     all systems.
     * @param callback Callback for operation.
     */
    void logIn(
        const nx::network::http::Credentials& credentials,
        const OptionalLocalPushSettings& confirmedSettings,
        Callback callback);

    /**
     * Disables push notifications at the remote side. Sends unsubscription request to the
     * cloud instance.
     */
    void logOut();

    /**
     * Explicitly replaces confirmed settings value. Sometimes we need it to perform proper reaction
     * on errors, for example. Does not send any remote requests, affects only internal state.
     */
    void replaceConfirmedSettings(const LocalPushSettings& settings);

    /**
     * Sends request with specified settings to the cloud instance.
     * @param settings Settings to be applied.
     * @param callback Callback for operation.
     */
    void tryUpdateRemoteSettings(
        const LocalPushSettings& settings,
        Callback callback = Callback());

    /**
     * @return True if push notification credentials are specified, otherwise false.
     */
    bool loggedIn() const;

signals:
    void userUpdateInProgressChanged();
    void confirmedSettingsChanged();
    void loggedInChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile::details
