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
    void sessionExpired();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
