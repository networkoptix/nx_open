// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::common { class SystemSettings; }

namespace nx::vms::client::core {

class RemoteSession;

class NX_VMS_CLIENT_CORE_API RemoteSessionTimeoutWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    /**
     * When temporary token is used for authorization, common session token is created and used
     * instead. This session token can wear off earlier than the temporary token, but for security
     * reasons we should ask user to re-enter temporary token manually.
     */
    enum class SessionExpirationReason
    {
        sessionExpired,
        temporaryTokenExpired
    };

public:
    RemoteSessionTimeoutWatcher(nx::vms::common::SystemSettings* globalSettings, QObject* parent = nullptr);
    virtual ~RemoteSessionTimeoutWatcher() override;

    void tick();

    void sessionStarted(std::shared_ptr<RemoteSession> session);
    void sessionStopped();

    /**
     * User hid notification. If it was the first notification, then the second one will be
     * displayed later.
     */
    void notificationHiddenByUser();

signals:
    void showNotification(std::chrono::minutes timeLeft);
    void hideNotification();
    void sessionExpired(SessionExpirationReason reason);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
