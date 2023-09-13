// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/utils/system_error.h>

namespace nx::vms::rules { class NotificationAction; }

namespace nx::vms::client::desktop {

class CrossSystemNotificationsListener: public QObject
{
    Q_OBJECT

public:
    explicit CrossSystemNotificationsListener(QObject* parent = nullptr);
    virtual ~CrossSystemNotificationsListener() override;

signals:
    void notificationActionReceived(
        const QSharedPointer<nx::vms::rules::NotificationAction>& notificationAction,
        const QString& cloudSystemId);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
