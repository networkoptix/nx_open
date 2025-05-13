// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/coro/task.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/cloud_api.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>

namespace nx::vms::client::core {

// Provides access to the current user profile data loaded from the cloud.
class CloudUserProfileWatcher: public QObject
{
    Q_OBJECT

    using base_type = QObject;

    Q_PROPERTY(CloudStatusWatcher* statusWatcher MEMBER m_statusWatcher NOTIFY statusWatcherChanged);

    Q_PROPERTY(QString fullName READ fullName NOTIFY fullNameChanged)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(bool accountBelongsToOrganization
        READ accountBelongsToOrganization
        NOTIFY accountBelongsToOrganizationChanged)

public:
    CloudUserProfileWatcher(QObject* parent = nullptr);
    virtual ~CloudUserProfileWatcher() override;

    QString fullName() const;
    QUrl avatarUrl() const;

    bool accountBelongsToOrganization() const;

protected:
    coro::FireAndForget run();

signals:
    void fullNameChanged();
    void avatarUrlChanged();
    void accountBelongsToOrganizationChanged();

    void statusWatcherChanged();

private:
    QPointer<CloudStatusWatcher> m_statusWatcher;
    QString m_fullName;
    bool m_accountBelongsToOrganization = false;
};

} // nx::vms::client::core
